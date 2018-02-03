#include <stdio.h>

#include "misc.h"
#include "init.h"

#include <libusb.h>

#define DEV_VID 0x1c7a
#define DEV_PID 0x0570
#define DEV_CONF 0x1
#define DEV_INTF 0x0

#define DEV_EPOUT 0x04
#define DEV_EPIN 0x83

void printData(unsigned char * data, int length)
{
	for (int i = 0; i < length; ++i)
		printf("%#04x ", data[i]);
	printf("\n");
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
		for (int y = 0; y < width; ++y, ++i)
		{
			fputc(data[i], fp);
			if (y < width - 1)
				fputc(' ', fp);
		}
		fputc('\n', fp);
	}

	fclose(fp);
}

void imgInfo(unsigned char * data, int length)
{
	int min = data[0], max = data[0];
	for (int i = 0; i < length; ++i)
	{
		if (data[i] < min)
			min = data[i];
		if (data[i] > max)
			max = data[i];
	}
	printf("min: %d | max: %d\n", min, max);
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
	// 1 bpp

	// 0.5 bpp
	/*writeImg("scans/min/0_32x2032.pbm", data, 32, 2032);
	writeImg("scans/min/0_64x1016.pbm", data, 64, 1016);
	writeImg("scans/min/0_127x512.pbm", data, 127, 512);
	writeImg("scans/min/0_128x508.pbm", data, 128, 508);
	writeImg("scans/min/0_254x256.pbm", data, 254, 256);

	writeImg("scans/min/0_2032x32.pbm", data, 2032, 32);
	writeImg("scans/min/0_1016x64.pbm", data, 1016, 64);
	writeImg("scans/min/0_512x127.pbm", data, 512, 127);
	writeImg("scans/min/0_508x128pbm", data, 508, 128);
	writeImg("scans/min/0_256x254.pbm", data, 256, 254);*/

	int repeatLen = sizeof(repeat) / sizeof(repeat[0]);
	int repeatPktSize = sizeof(init[0]);
	while (1)
	{
		for (int i = 0; i < repeatLen; ++i)
		{
			if (libusb_bulk_transfer(handle, DEV_EPOUT, repeat[i], repeatPktSize, &transferred, 0) != 0)
				perror_exit("'Out' transfer error");

			if (libusb_bulk_transfer(handle, DEV_EPIN, data, length, &transferred, 0) != 0)
				perror_exit("'In' transfer error");
			//printf("Read %d\n", transferred);
		}
		//imgInfo(data, transferred);
		//printImg(data, 128, 254);
		writeImg("scans/254x128.pbm", data, 254, 128);
		writeImg("scans/256x127.pbm", data, 256, 127);
		writeImg("scans/508x64.pbm", data, 508, 64);
		writeImg("scans/1016x32.pbm", data, 1016, 32);
		writeImg("scans/2032x16.pbm", data, 2032, 16);

		writeImg("scans/128x254.pbm", data, 128, 254);
		writeImg("scans/127x256.pbm", data, 127, 256);
		writeImg("scans/64x508.pbm", data, 64, 508);
		writeImg("scans/32x1016.pbm", data, 32, 1016);
		writeImg("scans/16x2032.pbm", data, 16, 2032);
	}

	libusb_release_interface(handle, DEV_INTF);
	libusb_attach_kernel_driver(handle, DEV_INTF);
	libusb_close(handle);
	libusb_exit(cntx);
	return 0;
}