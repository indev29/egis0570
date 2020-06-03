#include <stdio.h>
#include <stdlib.h>	

#include "misc.h"
#include "init.h"

#include <libusb.h>

#define DEV_VID 0x1c7a
#define DEV_PID 0x0570
#define DEV_CONF 0x1
#define DEV_INTF 0x0

#define DEV_EPOUT 0x04
#define DEV_EPIN 0x83

int _num = 0;

void printData(unsigned char * data, int length)
{
	for (int i = 0; i < length; ++i)
		printf("%#04x ", data[i]);
	printf("\n");
}

void finger_status(unsigned char * data)
{

}

void writeRaw(const char * filename, unsigned char * data, int length)
{
	FILE * fp = fopen(filename, "w");
	if (fp == NULL)
	{
		perror("Error opening file");
		return;
	}
	fwrite(data, sizeof(unsigned char), length, fp);

	fclose(fp);
}

void writeImg(const char * filename, unsigned char * data, int width, int height)
{
	const char format_mark[] = "P5";

	FILE * fp = fopen(filename, "w");
	if (fp == NULL)
	{
		perror("Error opening file");
		return;
	}

	fprintf(fp, "%s\n%d %d\n%d\n", format_mark, width, height, 255);

	/*for (int i = 0; i < width * height / 2; ++i)
	{
		fputc(data[i] & 0x0F, fp);
		fputc(' ', fp);
		fputc(data[i] & 0xF0, fp);
		fputc(' ', fp);
	}*/
    int i = 0;
	for (int x = 0; x < height; ++x)
	{
		for (int y = 0; y < width-1; ++y, ++i)
		{
			fputc(data[i], fp);
			/*  if (y < width - 1)
				fputc(' ', fp); */
		}
		fputc('\n', fp);
	}

	fclose(fp);
}

void imgInfo(unsigned char * data, int length)
{
	int res = 0;
	int min = data[0], max = data[0];
	for (int i = 0; i < length; ++i)
	{
		res += data[i];
		if (data[i] < min)
			min = data[i];
		if (data[i] > max)
			max = data[i];
	}
	printf("min: %d | max: %d | avg: %d\n", min, max, res / length);
}

int main(int argc, char * argv[])
{
	libusb_context * cntx = NULL;
	if (libusb_init(&cntx) != 0)
		perror_exit("Could not init context");

	//libusb_set_debug(cntx, LIBUSB_LOG_LEVEL_DEBUG);

	libusb_device_handle * handle = libusb_open_device_with_vid_pid(cntx, DEV_VID, DEV_PID);
	if (handle == NULL)
		perror_exit("Device not opened");

	if (libusb_kernel_driver_active(handle, DEV_INTF) == 1)
		libusb_detach_kernel_driver(handle, DEV_INTF);

	if (libusb_set_configuration(handle, DEV_CONF) != 0)
		perror_exit("Could not set configuration");

	if (libusb_claim_interface(handle, DEV_INTF) != 0)
		perror_exit("Could not claim device interface");

	libusb_reset_device(handle);

	// TRANSFER
	
	int initLen = sizeof(init) / sizeof(init[0]);
	int initPktSize = sizeof(init[0]);
	unsigned char data[32512];
	int length = sizeof(data);
	int transferred;
	for (int i = 0; i < initLen; ++i)
	{
		if (libusb_bulk_transfer(handle, DEV_EPOUT, init[i], initPktSize, &transferred, 0) != 0)
			perror_exit("'Out' transfer error");

		if (libusb_bulk_transfer(handle, DEV_EPIN, data, length, &transferred, 0) != 0)
			perror_exit("'In' transfer error");
		//printf("Read %d\n", transferred);
	}
	imgInfo(data, transferred);

    /* The proper image size is 114*57 and 5 image  */
	writeImg("egis0570_fingerprint.pgm", data, 115, 284);
	
	libusb_release_interface(handle, DEV_INTF);
	libusb_attach_kernel_driver(handle, DEV_INTF);
	libusb_close(handle);
	libusb_exit(cntx);
	return 0;
}
