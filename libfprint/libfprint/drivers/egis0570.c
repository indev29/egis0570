#define FP_COMPONENT "egis0570"

#include <errno.h>
#include <string.h>
#include <libusb.h>
#include <fp_internal.h>
#include <glib.h>

#include "egis0570.h"
#include "driver_ids.h"

struct egis_dev
{
	gboolean running;
	gboolean stop;

	gboolean retry;
	struct fp_img * img;

	int pkt_num;
	int pkt_type;
};

/* Packet types */
#define PKT_TYPE_INIT	 0
#define PKT_TYPE_REPEAT	 1

/*
 * Service
 */

static gboolean is_last_pkt(struct egis_dev * egdev)
{
	int type = egdev -> pkt_type;
	int num = egdev -> pkt_num;
	gboolean r;
	r = ((type == PKT_TYPE_INIT) && (num == (EGIS0570_INIT_TOTAL - 1)));
	r |= ((type == PKT_TYPE_REPEAT) && (num == (EGIS0570_REPEAT_TOTAL - 1)));
	return r;
}

static gboolean finger_status(struct fp_img * img)
{
	size_t total = 0;
	unsigned char min, max;
	min = max = img -> data[0];
	for (size_t i = 0; i < img -> length; ++i)
	{
		total += img -> data[i];
		if (img -> data[i] < min)
			min = img -> data[i];
		if (img -> data[i] > max)
			max = img -> data[i];
	}

	unsigned char avg = total / img -> length;
	gboolean result = ((avg > 210) && (min < 130));

	fp_dbg("Finger status: %d : %d - %d", min, max, avg);

	return result;
}

/*
 * SSM States
 */

static void recv_data_resp(struct fpi_ssm *);
static void recv_cmd_resp(struct fpi_ssm *);
static void send_cmd_req(struct fpi_ssm *, unsigned char *);

enum sm_states
{
	SM_START,
	SM_REQ,
	SM_RESP,
	SM_RESP_CB,
	SM_DATA,
	SM_DATA_PROC,
	SM_STATES_NUM
};

static void state_complete(struct fpi_ssm *ssm, gboolean img_free)
{
	struct fp_img_dev *dev = ssm->priv;
	struct egis_dev * egdev = dev->priv;
	int r = ssm -> error;

	fpi_ssm_free(ssm);
	if (img_free)
		fp_img_free(egdev->img);
	egdev -> img = NULL;
	egdev -> running = FALSE;
	egdev -> retry = FALSE;
	if (r)
		fpi_imgdev_session_error(dev, r);

	if (egdev -> stop)
		fpi_imgdev_deactivate_complete(dev);
}

static void ssm_run_state(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;
	
	switch (ssm -> cur_state)
	{
		case SM_START:
			egdev -> running = TRUE;
			egdev -> pkt_num = 0;
			fpi_ssm_next_state(ssm);
			break;
		case SM_REQ:
			if (egdev -> pkt_type == PKT_TYPE_INIT)
				send_cmd_req(ssm, init_pkts[egdev -> pkt_num]);
			else
				send_cmd_req(ssm, repeat_pkts[egdev -> pkt_num]);
			break;
		case SM_RESP:
			if (is_last_pkt(egdev) == FALSE)
			{
				recv_cmd_resp(ssm);
				++(egdev->pkt_num);
			}
			else
				fpi_ssm_jump_to_state(ssm, SM_DATA);
			break;
		case SM_RESP_CB:
			fpi_ssm_jump_to_state(ssm, SM_REQ);
			break;
		case SM_DATA:
			recv_data_resp(ssm);
			break;
		case SM_DATA_PROC:
			break;
	}
}

/*
 * Device communication
 */

static void data_resp_cb(struct libusb_transfer * transfer)
{
	struct fpi_ssm * ssm = transfer -> user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
	{
		fp_err("Transfer is not completed");
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}
	
	fpi_ssm_next_state(ssm);
out:
	libusb_free_transfer(transfer);
}

static void recv_data_resp(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	struct libusb_transfer * transfer = libusb_alloc_transfer(0);

	struct fp_img * img = fpi_img_new_for_imgdev(dev);
	egdev->img = img;

	libusb_fill_bulk_transfer(transfer, dev->udev, EGIS0570_EPIN, img->data, img->length, data_resp_cb, ssm, EGIS0570_TIMEOUT);

	int r = libusb_submit_transfer(transfer);
	if (r != 0)
	{
		fp_img_free(img);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void activation_start(struct fp_img_dev *);

static void cmd_resp_cb(struct libusb_transfer * transfer)
{
	struct fpi_ssm * ssm = transfer -> user_data;
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
	{
		fp_err("Transfer is not completed");
		/*
		 * Fixing weird enroll behaviour, awful solution
		 * Spent couple of days here, no idea what else can i do
		 * Would be good if someone fix this better way
		 */
		if (egdev->retry == FALSE)
		{
			libusb_reset_device(dev->udev);
			state_complete(ssm, FALSE);
			activation_start(dev);
			egdev->retry = TRUE;
		}
		else
			fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}

	fpi_ssm_jump_to_state(ssm, SM_RESP_CB);
out:
	g_free(transfer -> buffer);
	libusb_free_transfer(transfer);

}

static void recv_cmd_resp(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;

	struct libusb_transfer * transfer = libusb_alloc_transfer(0);
	unsigned char * data = g_malloc(EGIS0570_PKTSIZE);
	int length = EGIS0570_PKTSIZE;

	libusb_fill_bulk_transfer(transfer, dev->udev, EGIS0570_EPIN, data, length, cmd_resp_cb, ssm, EGIS0570_TIMEOUT);

	int r = libusb_submit_transfer(transfer);
	if (r != 0)
	{
		g_free(data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void cmd_req_cb(struct libusb_transfer * transfer)
{
	struct fpi_ssm * ssm = transfer -> user_data;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
	{
		fp_err("Transfer is not completed");
		fpi_ssm_mark_aborted(ssm, -EIO);
		goto out;
	}
	
	fpi_ssm_next_state(ssm);
out:
	libusb_free_transfer(transfer);
}

static void send_cmd_req(struct fpi_ssm * ssm, unsigned char * pkt)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	int length = EGIS0570_PKTSIZE;

	if (!transfer)
	{
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	libusb_fill_bulk_transfer(transfer, dev->udev, EGIS0570_EPOUT, pkt, length, cmd_req_cb, ssm, EGIS0570_TIMEOUT);

	int r = libusb_submit_transfer(transfer);
	if (r != 0)
	{
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

/*
 * Capture
 */

static void fcheck_start(struct fp_img_dev *);

static void capture_run_state(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	ssm_run_state(ssm);

	switch (ssm -> cur_state)
	{
		case SM_START:
			egdev->pkt_type = PKT_TYPE_REPEAT;
			break;
		case SM_DATA_PROC:
			fpi_imgdev_image_captured(dev, egdev -> img);
			fpi_imgdev_report_finger_status(dev, FALSE);
			fpi_ssm_next_state(ssm);
			break;
	}	
}

static void capture_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;
	state_complete(ssm, FALSE);

	if (egdev -> stop == FALSE)
		fcheck_start(dev);
}

static void capture_start(struct fp_img_dev *dev)
{
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, capture_run_state, SM_STATES_NUM);
	ssm -> priv = dev;

	fpi_ssm_start(ssm, capture_complete);
}

/*
 * Finger check
 */

static void fcheck_run_state(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	ssm_run_state(ssm);

	switch (ssm -> cur_state)
	{
		case SM_START:
			egdev->pkt_type = PKT_TYPE_REPEAT;
			break;
		case SM_DATA_PROC:
			if (finger_status(egdev->img))
			{
				fpi_imgdev_report_finger_status(dev, TRUE);
				fp_img_free(egdev->img);
				egdev -> img = NULL;
				fpi_ssm_next_state(ssm);
			}
			else
				fpi_ssm_jump_to_state(ssm, SM_START);
			break;
	}
	
}

static void fcheck_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	state_complete(ssm, TRUE);

	if (egdev -> stop == FALSE)
		capture_start(dev);
}

static void fcheck_start(struct fp_img_dev *dev)
{
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, fcheck_run_state, SM_STATES_NUM);
	ssm -> priv = dev;

	fpi_ssm_start(ssm, fcheck_complete);
}

/*
 * Activation
 */

static void activation_run_states(struct fpi_ssm *ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	ssm_run_state(ssm);

	switch (ssm -> cur_state)
	{
		case SM_START:
			egdev->pkt_type = PKT_TYPE_INIT;
			break;
		case SM_DATA_PROC:
			fpi_ssm_next_state(ssm);
			break;
	}
}

static void activation_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;
	state_complete(ssm, TRUE);

	if (egdev -> stop == FALSE)
		fcheck_start(dev);
}

static void activation_start(struct fp_img_dev *dev)
{
	struct egis_dev * egdev = dev->priv;
	egdev -> stop = FALSE;

	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, activation_run_states, SM_STATES_NUM);
	ssm -> priv = dev;

	fpi_ssm_start(ssm, activation_complete);

	fpi_imgdev_activate_complete(dev, 0);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	activation_start(dev);
	return 0;
}

/*
 * Deactivation
 */

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct egis_dev * egdev = dev->priv;
	if (egdev -> running)
		egdev -> stop = TRUE;
	else
		fpi_imgdev_deactivate_complete(dev);
}

/*
 * Opening
 */

static int dev_open(struct fp_img_dev *dev, unsigned long driver_data)
{
	int r;

	r = libusb_set_configuration(dev->udev, EGIS0570_CONF);
	if (r != 0)
	{
		fp_err("could set configuration %d: %s", EGIS0570_CONF, libusb_error_name(r));
		return r;
	}

	r = libusb_claim_interface(dev->udev, EGIS0570_INTF);
	if (r != 0)
	{
		fp_err("could not claim interface %d: %s", EGIS0570_INTF, libusb_error_name(r));
		return r;
	}

	/* 
	 * WARNING: Device reseting is a very important part 
	 * I'd recommend NOT to touch this line
	 */
	libusb_reset_device(dev->udev);
	
	struct egis_dev * egdev = g_malloc0(sizeof(struct egis_dev));
	egdev -> running = FALSE;
	egdev -> stop = FALSE;
	egdev -> img = NULL;
	egdev -> retry = FALSE;
	dev -> priv = (void *)egdev;

	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

/*
 * Closing
 */

static void dev_close(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, EGIS0570_INTF);
	fpi_imgdev_close_complete(dev);
}

/*
 * Driver data
 */

static const struct usb_id id_table[] = {
	{ .vendor = EGIS0570_VID, .product = EGIS0570_PID },
	{ 0, 0, 0, },
};

struct fp_img_driver egis0570_driver = {
	.driver = {
		.id = EGIS0570_ID,
		.name = FP_COMPONENT,
		.full_name = "Egis Technology Inc. (aka. LighTuning) 0570",
		.id_table = id_table,
		.scan_type = FP_SCAN_TYPE_PRESS,
	},
	.flags = 0,
	.img_height = EGIS0570_IMGHEIGHT,
	.img_width = EGIS0570_IMGWIDTH,

	.bz3_threshold = EGIS0570_BZ3_THRESHOLD,
	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};