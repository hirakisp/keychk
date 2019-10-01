/*
 * USB-Keyboard scancode test
 *
 * Copyright (C) 2019 Seiji Hiraki
 *
 * Modern USB keyboards have many scan codes.
 * I made this software to support various scan codes.
 */

#include <usb.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static void keyboard_verify(char *filename, struct usb_device *dev);
static void keyboard_test(struct usb_device *dev);

#define BUFSIZE (256)
#define IS_KEYBOARD(alts)     (alts.bInterfaceClass == 3) && (alts.bInterfaceSubClass == 1) && (alts.bInterfaceProtocol == 1)

int main(int argc,char *argv[])
{
	uint8_t device_count = 0;
	struct usb_interface_descriptor alts;

	printf("argc = %d \n", argc);
	usb_init();
	usb_find_busses();
	usb_find_devices();

	for (struct usb_bus* bus = usb_get_busses(); bus; bus = bus->next) {
		for (struct usb_device* dev = bus->devices; dev; dev = dev->next) {
			device_count++;
			alts = dev->config[0].interface[0].altsetting[0];

			if (IS_KEYBOARD(alts)) {
				if (argc > 1)
					keyboard_verify((char *)argv[1], dev);
				else
					keyboard_test(dev);
				break;
			}
		}
	}

	if (device_count <= 0)
		printf("devices not found. \n");

	return 0;
}

static void keyboard_verify(char *filename, struct usb_device *dev)
{
	usb_dev_handle *handle;
	struct usb_interface_descriptor alts = dev->config[0].interface[0].altsetting[0];
	uint8_t ep = alts.endpoint[0].bEndpointAddress;
	int ep_size = alts.endpoint[0].wMaxPacketSize;
	FILE *fp;
	char line[BUFSIZE];
	char buf[BUFSIZE];
	char instr[BUFSIZE];
	int keyno = 0;
	ssize_t read_size;
	int i;
	int sum;
	int count[2]; // ok, ng

	handle = usb_open(dev);

	/* Get ownership of device */
	usb_claim_interface(handle, alts.bInterfaceNumber);

	/* Dummy read */
	read_size = usb_interrupt_read(handle, ep, (char *)buf, ep_size, 100);

	if (filename == NULL) {
		printf("File name is NULL\n");
		return;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		printf("File open error, Can't open %s \n", filename);
		return;
	}

	/* Read all lines of file */
	while ( fgets(line, BUFSIZE, fp) != NULL ) {
		keyno++;

		/* Remove return code */
		for (i = strlen(line) - 1; i > 0; i--) {
			if ((line[i] == '\r') || (line[i] == '\n'))
				line[strlen(line) - 1] = 0;
			else
				break;
		}

		/* Check assigned format  e.g. "00, 00, 00" */
		if ((strlen(line) >= 10) && (line[2] == ',') && (line[6] == ',')) {
			printf("%s line \n", line);
			printf("Enter keyno.%d  expect:%s \n", keyno, line);

			while (1) {
				instr[0] = 0;
				read_size = usb_interrupt_read(handle, ep, (char *)buf, ep_size, 100);
				if (read_size < 0) {
					if (read_size == -ETIMEDOUT)
						continue;
					else
						printf("read error: %ld\n", read_size);
				} else {
					/* Check all 0 */
					sum = 0;
					for (i = 0; i < ep_size; i++)
						sum += buf[i];
					if (sum == 0)
						continue;

					/* Check SIGINT (Ctrl + C) and escape while */
					if ((buf[0] & 0x11) && (buf[2] == 0x06)) {
						return;
					} else {
						for (i = 0; i < ep_size; i++) {
							if (i > 0)
								sprintf(instr + strlen(instr), ", ");
							sprintf(instr + strlen(instr), "%02X", buf[i]);
						}
					}
					printf("instr: %s ", instr);

					if (strncmp(instr, line, strlen(line)) == 0) {
						printf("OK!!!\n");
						count[0]++
						break;
					} else {
						printf("NG!!!\n");
						count[1]++;
					}
				}
			}
		}
	}

	fclose(fp);
	usb_resetep(handle, ep);
	usb_release_interface(handle, alts.bInterfaceNumber);
	usb_close(handle);

	printf("Keycode test finished. Count OK=%d NG=%d \n", count[0], count[1]);
}


static void keyboard_test(struct usb_device *dev)
{
	usb_dev_handle *handle;
	unsigned char buf[256];
	struct usb_interface_descriptor alts = dev->config[0].interface[0].altsetting[0];
	uint8_t ep = alts.endpoint[0].bEndpointAddress;
	int ep_size = alts.endpoint[0].wMaxPacketSize;
	ssize_t read_size;
	int i;

	handle = usb_open(dev);

	/*
	 * Show assigned device descriptor.
	 * The device descriptor of a USB device represents the entire device.
	 */
	printf("Information of Device Descpritors: \n");
	printf("idVendor: 0x%04x \n", dev->descriptor.idVendor);
	printf("idProduct: 0x%04x \n", dev->descriptor.idProduct);
	printf("bcdDevice: 0x%04x \n", dev->descriptor.bcdDevice);

	usb_get_string_simple(handle, dev->descriptor.iManufacturer, (char *)buf, sizeof(buf));
	printf("iManufacturer: %02d(%s) \n", dev->descriptor.iManufacturer, buf);

	usb_get_string_simple(handle, dev->descriptor.iProduct, (char *)buf, sizeof(buf));
	printf("iProduct: %02d(%s) \n", dev->descriptor.iProduct, buf);

	usb_get_string_simple(handle, dev->descriptor.iSerialNumber, (char *)buf, sizeof(buf));
	printf("iSerialNumber: %02d(%s) \n", dev->descriptor.iSerialNumber, buf);

	/* Get ownership of device */
	usb_claim_interface(handle, alts.bInterfaceNumber);

	while (1) {
		read_size = usb_interrupt_read(handle, ep, (char *)buf, ep_size, 100);
		if (read_size < 0) {
			if (read_size == -ETIMEDOUT)
				continue;
			else
				printf("read error: %ld\n", read_size);
		}

		/* Check SIGINT (Ctrl + C) and escape while */
		if ((buf[0] & 0x11) && (buf[2] == 0x06)) {
			break;
		} else {
			for (i = 0; i < ep_size; i++)
				printf("0x%02x ", buf[i]);
			printf("\n");
		}
	}

	usb_resetep(handle, ep);
	usb_release_interface(handle, alts.bInterfaceNumber);
	usb_close(handle);
}
