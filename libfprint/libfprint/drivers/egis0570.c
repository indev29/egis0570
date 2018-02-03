#define FP_COMPONENT "egis0570"

#include <errno.h>
#include <string.h>
#include <libusb.h>
#include <fp_internal.h>

#include "egis0570.h"
#include "driver_ids.h"

static struct egis_dev
{
	g_boolean running;
	g_boolean stop;
	struct * fp_img img;
}

static struct transfer_user_data
{
	struct fpi_ssm * ssm;
	pkt_types pkt_type;
	int pkt_num;
};

static enum loop_states
{
	LOOP_INIT,
	LOOP_INIT_CAPTURE_DONE,
	LOOP_REPEAT,
	LOOP_CAPTURE_DONE,
	LOOP_STATES_NUM
};

static enum pkt_types
{
	PKT_INIT,
	PKT_REPEAT
};

static g_boolean is_last_pkt(pkt_types type, int num)
{
	g_boolean r;
	r = ((type == PKT_INIT) && (num < (EGIS0570_INIT_TOTAL - 1)));
	r |= ((type == PKT_REPEAT) && (num < (EGIS0570_REPEAT_TOTAL - 1)));
	return r;
}

static struct transfer_user_data * alloc_transfer_data(struct fpi_ssm * ssm, pkt_types pkt_type, int pkt_num)
{
	struct transfer_user_data * cb_data = g_malloc0(sizeof(struct transfer_user_data));
	cb_data -> ssm = ssm;
	cb_data -> pkt_type = type;
	cb_data -> pkt_num = num;

	return cb_data;
}

static void * get_pkt_ptr(pkt_types type, int num)
{
	unsigned char * ptr;
	if (type == PKT_INIT)
		ptr = init_pkts[num];
	else
		ptr = repeat_pkts[num];

	return ptr;
}

static void recv_pkt_cb(struct libusb_transfer * transfer)
{
	struct transfer_user_data * cb_data = transfer -> user_data;
	struct fpi_ssm * ssm = cb_data -> ssm;
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);
	else
	{
		if (is_last_pkt(cb_data->pkt_type, cb_data->pkt_num) == FALSE)
		{
			g_free(transfer -> buffer); /* We won't free the buffer of the last packet as it contains img */
			send_pkt(ssm, cb_data->pkt_type, cb_data->pkt_num + 1);
		}
		else
		{
			/* Last packet normally means, that we've received image */
			struct fp_img * img = egdev -> img;
			egdev -> img = NULL;
			fpi_imgdev_report_finger_status(dev, 1); /*TEMPORARILY PASS 1 HERE, FINGER CHECK NEEDED*/
			fpi_imgdev_image_captured(dev, img);
			fpi_ssm_next_state(ssm);
		}
	}

	g_free(cb_data);
	libusb_free_transfer(transfer);
}

static void recv_pkt(struct fpi_ssm * ssm, pkt_types type, int num)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;
	struct libusb_transfer * transfer = libusb_alloc_transfer(0);
	unsigned char * data;
	int length;
	int r;

	if (!transfer)
	{
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}

	if (is_last_pkt(type, num) == FALSE)
	{
		data = g_malloc(EGIS0570_PKTSIZE);
		length = EGIS0570_PKTSIZE;
	}
	else
	{
		struct fp_img * img = fpi_img_new_for_imgdev(dev);
		egdev->img = img;
		data = img;
		length = img -> length;
	}
	struct transfer_user_data * cb_data = alloc_transfer_data(ssm, type, num);
	libusb_fill_bulk_transfer(transfer, dev->udev, EGIS0570_EPIN, data, length, recv_pkt_cb, cb_data, EGIS0570_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r != 0)
	{
		g_free(data);
		g_free(cb_data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void send_pkt_cb(struct libusb_transfer * transfer)
{
	struct transfer_user_data * cb_data = transfer -> user_data;
	struct fpi_ssm * ssm = cb_data -> ssm;
	pkt_types pkt_type = cb_data -> pkt_type;
	int pkt_num = cb_data -> pkt_num;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED)
		fpi_ssm_mark_aborted(ssm, -EIO);

	g_free(cb_data);
	libusb_free_transfer(transfer);

	recv_pkt(ssm, pkt_type, pkt_num);
}

static void send_pkt(struct fpi_ssm * ssm, pkt_types type, int num)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct libusb_transfer *transfer = libusb_alloc_transfer(0);
	unsigned char * data = get_pkt_ptr(type, num);
	int length = EGIS0570_PKTSIZE;
	int r;

	if (!transfer)
	{
		fpi_ssm_mark_aborted(ssm, -ENOMEM);
		return;
	}
	struct transfer_user_data * cb_data = alloc_transfer_data(ssm, type, num);
	libusb_fill_bulk_transfer(transfer, dev->udev, EGIS0570_EPOUT, data, length, send_pkt_cb, cb_data, EGIS0570_TIMEOUT);

	r = libusb_submit_transfer(transfer);
	if (r != 0)
	{
		g_free(cb_data);
		libusb_free_transfer(transfer);
		fpi_ssm_mark_aborted(ssm, r);
	}
}

static void state_loop(struct fpi_ssm * ssm)
{
	struct fp_img_dev * dev = ssm -> priv;
	struct egis_dev * egdev = dev -> priv;
	
	switch (ssm -> cur_state)
	{
		case LOOP_INIT:
			send_pkt(ssm, PKT_INIT, 0);
			break;
		case LOOP_INIT_CAPTURE_DONE:
			fpi_ssm_next_state(ssm);
			break;
		case LOOP_REPEAT:
			if (egdev -> stop)
				fpi_ssm_mark_completed(ssm);
			else
				send_pkt(ssm, PKT_REPEAT, 0);
			break;
		case LOOP_CAPTURE_DONE:
			fpi_ssm_jump_to_state(ssm, LOOP_REPEAT);
			break;
	}
}

static void sm_complete(struct fpi_ssm *ssm)
{
	struct fp_img_dev *dev = ssm->priv;
	struct egis_dev * egdev = dev->priv;
	int r = ssm -> error;

	fpi_ssm_free(ssm);
	fp_img_free(egdev->img);
	egdev -> img = NULL;
	egdev -> running = FALSE;
	if (r)
		fpi_imgdev_session_error(dev, r);

	if (egdev -> stop)
		fpi_imgdev_deactivate_complete(dev);
}

static int dev_activate(struct fp_img_dev *dev, enum fp_imgdev_state state)
{
	struct fpi_ssm *ssm = fpi_ssm_new(dev->dev, state_loop, LOOP_STATES_NUM);
	ssm -> priv = dev;
	struct egis_dev * egdev = dev -> priv; 
	egdev -> running = TRUE;

	fpi_ssm_start(ssm, sm_complete);

	fpi_imgdev_activate_complete(dev, 0);
	return 0;
}

static void dev_deactivate(struct fp_img_dev *dev)
{
	struct egis_dev *egdev = dev->priv;
	if (vdev -> running)
		vdev -> stop = TRUE;
	else
		fpi_imgdev_deactivate_complete(dev);
}

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

	/* Propably reset the device?
	libusb_reset_device(handle);
	*/
	struct egis_dev egdev = g_malloc0(struct egis_dev);
	egdev -> running = FALSE;
	egdev -> stop = FALSE;
	egdev -> img = NULL;
	dev -> priv = egdev;
	fpi_imgdev_open_complete(dev, 0);
	return 0;
}

static void dev_close(struct fp_img_dev *dev)
{
	g_free(dev->priv);
	libusb_release_interface(dev->udev, EGIS0570_INTF);
	fpi_imgdev_close_complete(dev);
}

static const struct usb_id id_table[] = {
	{ .vendor = EGIS0570_VID, .product = EGIS0570_PID },
	{ 0, 0, 0, },
};

struct fp_img_driver uru4000_driver = {
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

	.open = dev_open,
	.close = dev_close,
	.activate = dev_activate,
	.deactivate = dev_deactivate,
};