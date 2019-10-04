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

#define KEYMAX  (128)
#define CODE_BUFSIZE (256)
#define BUFSIZE (256)
#define IS_KEYBOARD(alts)     (alts.bInterfaceClass == 3) && (alts.bInterfaceSubClass == 1) && (alts.bInterfaceProtocol == 1)


struct KEYCODE {
	uint8_t number;
	char code[CODE_BUFSIZE];
};

static void keyboard_verify(struct usb_device *dev, struct KEYCODE *keycodes);
static void keyboard_test(struct usb_device *dev);
static int read_keychkorder(char *filename, struct KEYCODE *);
static int read_keycodes(char *filename, struct KEYCODE *);


int main(int argc,char *argv[])
{
	uint8_t device_count = 0;
	struct usb_interface_descriptor alts;
	struct KEYCODE keycodes[KEYMAX] = {0};

	printf("argc = %d \n", argc);
	usb_init();
	usb_find_busses();
	usb_find_devices();

	if (argc > 1) {
		if (read_keycodes((char *)argv[1], &keycodes[0]) != 0)
			return -1;
	}

	if (argc > 2) {
		if (read_keychkorder((char *)argv[2], &keycodes[0]) != 0)
			return -1;
	}

	for (struct usb_bus* bus = usb_get_busses(); bus; bus = bus->next) {
		for (struct usb_device* dev = bus->devices; dev; dev = dev->next) {
			device_count++;
			alts = dev->config[0].interface[0].altsetting[0];
			printf("found device\n");
			if (IS_KEYBOARD(alts)) {
				if (argc > 1)
					keyboard_verify(dev, &keycodes[0]);
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

static int read_keycodes(char *filename, struct KEYCODE *out)
{
	FILE *fp;
	char line[CODE_BUFSIZE] = {0};
	int keyno = 0;
	int i;

	printf("--- Read start [%s] ---\n", filename);

	if (filename == NULL) {
		printf("File name is NULL\n");
		return -1;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		printf("File open error, Can't open %s \n", filename);
		return -1;
	}

	/* Read all lines of file */
	while ( fgets(line, CODE_BUFSIZE, fp) != NULL ) {
		/* Remove return code */
		for (i = strlen(line) - 1; i > 0; i--) {
			if ((line[i] == '\r') || (line[i] == '\n'))
				line[strlen(line) - 1] = 0;
			else
				break;
		}
		out[keyno].number = keyno + 1;
		strcpy(out[keyno].code, line);
		printf(" %s\n", out[keyno].code);
		keyno++;
	}

	printf("--- Read OK ---\n");
	fclose(fp);

	return 0;
}

static int read_keychkorder(char *filename, struct KEYCODE *out)
{
	FILE *fp;
	char line[CODE_BUFSIZE];
	struct KEYCODE keycodes[KEYMAX] = {0};
	int keyno = 0;
	int rd_keyno = 0;

	printf("--- Read start [%s] --- \n ", filename);

	if (filename == NULL) {
		printf("File name is NULL\n");
		return -1;
	}

	if ((fp = fopen(filename, "r")) == NULL) {
		printf("File open error, Can't open %s \n", filename);
		return -1;
	}

	/* Copy original keycodes */
	memcpy(keycodes, out, sizeof(keycodes));

	/* Read all lines of file */
	while ( fgets(line, CODE_BUFSIZE, fp) != NULL ) {
		rd_keyno = strtol(line, NULL, 10);
		printf("%d \n", rd_keyno);

		out[keyno].number = keycodes[rd_keyno - 1].number;
		strcpy(out[keyno].code, keycodes[rd_keyno - 1].code);
		keyno++;
	}

	/* Clear remain key */
	for ( ; keyno < KEYMAX; keyno++) {
		out[keyno].number = 0;
		out[keyno].code[0] = 0;
	}
	fclose(fp);
	printf("--- Read OK ---\n");
	return 0;
}

static void keyboard_verify(struct usb_device *dev, struct KEYCODE *keycodes)
{
	usb_dev_handle *handle;
	struct usb_interface_descriptor alts = dev->config[0].interface[0].altsetting[0];
	uint8_t ep = alts.endpoint[0].bEndpointAddress;
	int ep_size = alts.endpoint[0].wMaxPacketSize;
	char buf[BUFSIZE];
	char instr[BUFSIZE];
	int keyno = 0;
	ssize_t read_size;
	int sum, i;
	int count[2] = {0}; // ok, ng
	char *line;

	handle = usb_open(dev);

	/* Get ownership of device */
	usb_claim_interface(handle, alts.bInterfaceNumber);

	/* Dummy read */
	read_size = usb_interrupt_read(handle, ep, (char *)buf, ep_size, 100);

	printf("--- Verify Start! ---\n");
	printf("* Press instructed key. * \n");
	printf("* You can abort verification when press Ctrl + C * \n");

	/* Read all lines of file */
	for (keyno = 0; keyno < KEYMAX; keyno++) {
		line = &keycodes[keyno].code[0];

		/* Skip */
		if (line[0] == 0)
			continue;

		printf(" [keyno.%d]\n", keycodes[keyno].number);

		/* Check assigned format  e.g. "00, 00, 00" */
		if ((strlen(line) >= 10) && (line[2] == ',') && (line[6] == ',')) {
			printf(" %s \n", line);

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
							sprintf(instr + strlen(instr), "%02X", buf[i] & 0xFF);
						}
					}
					printf(" %s ", instr);

					if (strncmp(instr, line, strlen(line)) == 0) {
						printf("\e[32m OK!!!\n\e[m");
						count[0]++;
						break;
					} else {
						printf("\e[31m NG!!!\n\e[m");
						count[1]++;
					}
				}
			}
		} else {
			printf(" Nocode key, Skip. \n");
		}
	}

	usb_resetep(handle, ep);
	usb_release_interface(handle, alts.bInterfaceNumber);
	usb_close(handle);

	printf("--- Keycode test finished. --- \n");
	printf(" Count OK=%d NG=%d \n", count[0], count[1]);
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
