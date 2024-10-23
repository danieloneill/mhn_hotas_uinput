#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>

#include <libusb-1.0/libusb.h>

#include <linux/uinput.h>
#include <linux/joystick.h>

#define HORI_VENDOR_ID          0x06d3
#define HORI_PRODUCT_ID         0x0f10

#define HORI_POLL_VR0           0x00
#define HORI_POLL_VR1           0x01
#define HORI_POLL_VR2           0x02

#define HORI_HAT_X      ABS_TILT_X // or ABS_RX
#define HORI_HAT_Y      ABS_TILT_Y // or ABS_RY

struct hori_input_ir {
	uint8_t	pos_x;
	uint8_t	pos_y;

	uint8_t	rudder;
	uint8_t	throttle;

	uint8_t	hat_x;
	uint8_t	hat_y;

	uint8_t	btn_a;
	uint8_t	btn_b;
} state_ir;

/* input: vendor request 0x00 */
struct hori_input_vr_00 {
	uint8_t fire_c : 1;           /* button fire-c */
	uint8_t button_d : 1;         /* button D */
	uint8_t hat : 1;              /* hat press */
	uint8_t button_st : 1;        /* button ST */

	uint8_t dpad1_top : 1;        /* d-pad 1 top */
	uint8_t dpad1_right : 1;      /* d-pad 1 right */
	uint8_t dpad1_bottom : 1;     /* d-pad 1 bottom */
	uint8_t dpad1_left : 1;       /* d-pad 1 left */

	uint8_t reserved1 : 4;

	uint8_t reserved2 : 1;
	uint8_t launch : 1;           /* button lauch */
	uint8_t trigger : 1;          /* trigger */
	uint8_t reserved3 : 1;
} state_vr00;

/* input: vendor request 0x01 */
struct hori_input_vr_01 {
	uint8_t reserved1 : 4;

	uint8_t dpad3_right : 1;      /* d-pad 3 right */
	uint8_t dpad3_middle : 1;     /* d-pad 3 middle */
	uint8_t dpad3_left : 1;       /* d-pad 3 left */
	uint8_t reserved2 : 1;

	uint8_t mode_select : 2;      /* mode select (M1 - M2 - M3, 2 - 1 - 3) */
	uint8_t reserved3 : 1;
	uint8_t button_sw1 : 1;       /* button sw-1 */

	uint8_t dpad2_top : 1;        /* d-pad 2 top */
	uint8_t dpad2_right : 1;      /* d-pad 2 right */
	uint8_t dpad2_bottom : 1;     /* d-pad 2 bottom */
	uint8_t dpad2_left : 1;       /* d-pad 2 left */
} state_vr01;

int uinp_fd;
struct uinput_user_dev uinp;

libusb_device_handle *hori_usbdev;

struct s_hori_usbinfo {
	uint8_t	address;
	uint8_t	type;
	uint16_t	maxpacket;
	uint8_t	interval;
} hori_usbinfo;

int hori_setup_uinput_abs(int code)
{
	struct uinput_abs_setup setup;
	setup.code = code;
	setup.absinfo.minimum = 0;
	setup.absinfo.maximum = 255;
	setup.absinfo.fuzz = 0;
	setup.absinfo.flat = 0;
	setup.absinfo.resolution = 0;
	setup.absinfo.value = 0;
	ioctl(uinp_fd, UI_ABS_SETUP, &setup);
}

int hori_init_uinput()
{
	int err;
	uinp_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(uinp_fd < 0) {
		fprintf(stderr, "hori_init_uinput: open(\"/dev/uinput\") failed: %s\n",
				strerror(uinp_fd));
		return -1;
	}

	err = ioctl(uinp_fd, UI_SET_EVBIT, EV_SYN);
	if(err < 0) {
		fprintf(stderr, "hori_init_uinput: ioctl(%d, UI_SET_EVBIT, EV_SYN) failed: %s\n",
				uinp_fd, strerror(err));
		return -1;
	}

	err = ioctl(uinp_fd, UI_SET_EVBIT, EV_KEY);
	if(err < 0) {
		fprintf(stderr, "hori_init_uinput: ioctl(%d, UI_SET_EVBIT, EV_KEY) failed: %s\n",
				uinp_fd, strerror(err));
		return -1;
	}

	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY1);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY2);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY3);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY4);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY5);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY6);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY7);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY8);

	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_THUMB);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_THUMB2);

	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_A);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_B);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_C);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_Y);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_X);

	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_Z);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TL);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_TR);
	ioctl(uinp_fd, UI_SET_KEYBIT, BTN_MODE);

	ioctl(uinp_fd, UI_SET_EVBIT, EV_ABS);
	ioctl(uinp_fd, UI_SET_ABSBIT, ABS_X);
	ioctl(uinp_fd, UI_SET_ABSBIT, ABS_Y);
	ioctl(uinp_fd, UI_SET_ABSBIT, ABS_RUDDER);
	ioctl(uinp_fd, UI_SET_ABSBIT, ABS_GAS);
	ioctl(uinp_fd, UI_SET_ABSBIT, HORI_HAT_X);
	ioctl(uinp_fd, UI_SET_ABSBIT, HORI_HAT_Y);

	memset(&uinp, 0, sizeof(uinp));
	snprintf(uinp.name, UINPUT_MAX_NAME_SIZE, "Mitsubishi HORI/Namco Flightstick 2");
	uinp.id.bustype = BUS_USB;
	uinp.id.vendor = HORI_VENDOR_ID;
	uinp.id.product = HORI_PRODUCT_ID;
	uinp.id.version = 1;

	err = write(uinp_fd, &uinp, sizeof(uinp));
	if(err < 0) {
		fprintf(stderr, "hori_init_uinput: write(%d, uinp) failed: %s\n",
				uinp_fd, strerror(err));
		return -1;
	}

	hori_setup_uinput_abs(ABS_X);
	hori_setup_uinput_abs(ABS_Y);
	hori_setup_uinput_abs(ABS_RUDDER);
	hori_setup_uinput_abs(ABS_GAS);
	hori_setup_uinput_abs(HORI_HAT_X);
	hori_setup_uinput_abs(HORI_HAT_Y);

	err = ioctl(uinp_fd, UI_DEV_CREATE);
	if(err < 0) {
		fprintf(stderr, "hori_init_uinput: ioctl(%d, UI_DEV_CREATE) failed: %s\n",
				uinp_fd, strerror(err));
		return -1;
	}

	return 0;
}

void hori_shutdown_uinput()
{
	if(ioctl(uinp_fd, UI_DEV_DESTROY) < 0) {
		fprintf(stderr, "hori_shutdown_uinput: ioctl(%d, UI_DEV_DESTROY) failed.\n", uinp_fd);
	}
	close(uinp_fd);
}

int hori_emit_uinput(int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;

	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	int ret = write(uinp_fd, &ie, sizeof(ie));
	if( ret < 0 ) {
		fprintf(stderr, "hori_emit_uinput: write(%d, data, %d) failed.\n",
				uinp_fd, sizeof(ie));
		return -1;
	}
	return 0;
}

int hori_init_usb()
{
	libusb_init(NULL);
	hori_usbdev = libusb_open_device_with_vid_pid(NULL, HORI_VENDOR_ID, HORI_PRODUCT_ID);
	if(!hori_usbdev) {
		fprintf(stderr, "hori_init_usb: Device could not be opened. Check that it is connected and that you have appropriate permissions.\n");
		return -1;
	}
	
	int err = libusb_claim_interface(hori_usbdev, 0);
	if(err) {
		fprintf(stderr, "hori_init_usb: Cannot claim interface: %s", libusb_strerror(err));
		return -1;
	}

	err = libusb_set_interface_alt_setting(hori_usbdev, 0, 0);
	if(err) {
		fprintf(stderr, "hori_init_usb: Cannot set alternate setting: %s", libusb_strerror(err));
		return -1;
	}

	/**
	 * Walk the 1 interface's 1 altsetting's 1 endpoint to get the address.
	 *
	 * We "know" it's 0x81, but... enh, just in case somebody wants to:
	 **/

	hori_usbinfo.address = 0x81;
/*
	libusb_device *device = libusb_get_device(hori_usbdev);
	struct libusb_config_descriptor *hori_usbconfig;
	err = libusb_get_active_config_descriptor(device, &hori_usbconfig);
	if(err) {
		fprintf(stderr, "hori_init_usb: Cannot get current config descriptor: %s", libusb_strerror(err));
		return -1;
	}
	for( int a=0; a < hori_usbconfig->bNumInterfaces; a++ )
	{
		const struct libusb_interface *iface = &hori_usbconfig->interface[a];
		for( int b=0; b < iface->num_altsetting; b++ )
		{
			const struct libusb_interface_descriptor *desc = &iface->altsetting[b];
			for( int c=0; c < desc->bNumEndpoints; c++ )
			{
				const struct libusb_endpoint_descriptor *ep = &desc->endpoint[c];
				hori_usbinfo.address = ep->bEndpointAddress;
				hori_usbinfo.type = ep->bDescriptorType;
				hori_usbinfo.maxpacket = ep->wMaxPacketSize;
				hori_usbinfo.interval = ep->bInterval;
			}
		}
	}
	libusb_free_config_descriptor( hori_usbconfig );
*/

	return 0;
}

int hori_relbtn_ir(int oldval, int newval, int btnid)
{
	if( oldval == newval )
		return 0;

	int oldstate = (oldval > 0xca ? 1 : 0);
	int newstate = (newval > 0xca ? 1 : 0);
	if( oldstate == newstate )
		return 0;

	if( 0 > hori_emit_uinput(EV_KEY, btnid, !newstate) ) {
		fprintf(stderr, "hori_relbtn_ir: hori_emit_uinput failed!\n");
		return -1;
	}

	return 1;
}

#define ABS_IR(FIELD, KEY) if( state_ir.FIELD != ir.FIELD ) { doreport = 1; state_ir.FIELD = ir.FIELD; if( 0 > hori_emit_uinput(EV_ABS, KEY, ir.FIELD) ) { fprintf(stderr, "ABS_IR: hori_emit_uinput failed!\n"); return -1; } }

int hori_poll_ir()
{
	int transferred;
	struct hori_input_ir ir;

	int err = libusb_interrupt_transfer(hori_usbdev, hori_usbinfo.address, (unsigned char *)&ir, sizeof(ir), &transferred, 200);

	// enh.
	if( LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 != err) {
		fprintf(stderr, "hori_poll_ir: libusb_interrupt_transfer failed: %d: %s", err, libusb_strerror(err));
		return -1;
	}

	if( transferred != sizeof(ir) ) {
		fprintf(stderr, "hori_poll_ir: Expected %d bytes, received %d bytes.\n",
				sizeof(ir), transferred);
		return 0;
	}

	int doreport = 0;
	ABS_IR(pos_x, ABS_X);
	ABS_IR(pos_y, ABS_Y);
	ABS_IR(rudder, ABS_RUDDER);
	ABS_IR(throttle, ABS_GAS);
	ABS_IR(hat_x, HORI_HAT_X);
	ABS_IR(hat_y, HORI_HAT_Y);

	int btn_a_ret = hori_relbtn_ir(state_ir.btn_a, ir.btn_a, BTN_A);
	state_ir.btn_a = ir.btn_a;
	if( btn_a_ret == 1 ) doreport = 1;
	if( btn_a_ret < 0 ) return btn_a_ret;

	int btn_b_ret = hori_relbtn_ir(state_ir.btn_b, ir.btn_b, BTN_B);
	state_ir.btn_b = ir.btn_b;
	if( btn_b_ret == 1 ) doreport = 1;
	if( btn_b_ret < 0 ) return btn_b_ret;

	if( 1 == doreport )
		return hori_emit_uinput(EV_SYN, SYN_REPORT, 0);

	return err;
}

#define KEY_VR00(FIELD, KEY) if( state_vr00.FIELD != vr.FIELD ) { doreport = 1; state_vr00.FIELD = vr.FIELD; if( 0 > hori_emit_uinput(EV_KEY, KEY, !vr.FIELD) ) { fprintf(stderr, "KEY_VR_00: hori_emit_uinput failed!\n"); return -1; } }
int hori_poll_vr_00()
{
	struct hori_input_vr_00 vr;

	int err = libusb_control_transfer(hori_usbdev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_ENDPOINT,
			HORI_POLL_VR0, 0, 1, (unsigned char *)&vr, sizeof(vr), 200);

	// Handle stall. (Do nothing, we're in a control xfer.)
	if( LIBUSB_ERROR_PIPE == err || LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 > err) {
		fprintf(stderr, "hori_poll_vr_00: libusb_control_transfer failed: %d: %s", err, libusb_strerror(err));
		return -1;
	}

	if( err != sizeof(vr) ) {
		fprintf(stderr, "hori_poll_vr_00: Expected %d bytes, received %d bytes.\n",
				sizeof(vr), err);
		return 0;
	}

	int doreport = 0;

	KEY_VR00(fire_c,	BTN_TRIGGER_HAPPY1);
	KEY_VR00(button_d,	BTN_TRIGGER_HAPPY2);
	KEY_VR00(hat,		BTN_TRIGGER_HAPPY3);
	KEY_VR00(button_st,	BTN_TRIGGER_HAPPY4);

	KEY_VR00(dpad1_top,	BTN_TRIGGER_HAPPY5);
	KEY_VR00(dpad1_right,	BTN_TRIGGER_HAPPY6);
	KEY_VR00(dpad1_bottom,	BTN_TRIGGER_HAPPY7);
	KEY_VR00(dpad1_left,	BTN_TRIGGER_HAPPY8);

	KEY_VR00(launch,	BTN_THUMB);
	KEY_VR00(trigger,	BTN_TRIGGER);

	if( 1 == doreport )
		return hori_emit_uinput(EV_SYN, SYN_REPORT, 0);

	return 0;
}

#define KEY_VR01(FIELD, KEY) if( state_vr01.FIELD != vr.FIELD ) { doreport = 1; state_vr01.FIELD = vr.FIELD; if( 0 > hori_emit_uinput(EV_KEY, KEY, !vr.FIELD) ) { fprintf(stderr, "KEY_VR_01: hori_emit_uinput failed!\n"); return -1; } }
int hori_poll_vr_01()
{
	struct hori_input_vr_01 vr;

	int err = libusb_control_transfer(hori_usbdev,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_ENDPOINT,
			HORI_POLL_VR1, 0, 1, (unsigned char *)&vr, sizeof(vr), 200);

	// Handle stall. (Do nothing, we're in a control xfer.)
	if( LIBUSB_ERROR_PIPE == err || LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 > err) {
		fprintf(stderr, "hori_poll_vr_01: libusb_control_transfer failed: %d: %s", err, libusb_strerror(err));
		return -1;
	}

	if( err != sizeof(vr) ) {
		fprintf(stderr, "hori_poll_vr_01: Expected %d bytes, received %d bytes.\n",
				sizeof(vr), err);
		return 0;
	}

	int doreport = 0;

	KEY_VR01(dpad3_right,	BTN_THUMB2);
	KEY_VR01(dpad3_middle,	BTN_C);
	KEY_VR01(dpad3_left,	BTN_X);

	KEY_VR01(button_sw1,	BTN_Y);

	KEY_VR01(dpad2_top,	BTN_Z);
	KEY_VR01(dpad2_right,	BTN_TL);
	KEY_VR01(dpad2_bottom,	BTN_TR);
	KEY_VR01(dpad2_left,	BTN_MODE);

	if( 1 == doreport )
		return hori_emit_uinput(EV_SYN, SYN_REPORT, 0);

	return 0;
}


int main(int argc, char **argv)
{
	int ret;

	memset(&state_ir, 0, sizeof(state_ir));
	memset(&state_vr00, 0, sizeof(state_vr00));
	memset(&state_vr01, 0, sizeof(state_vr01));

	ret = hori_init_uinput();
	if( 0 != ret)
		return ret;

	ret = hori_init_usb();
	if( 0 != ret ) {
		hori_shutdown_uinput();
		return ret;
	}

	int err = 0;
	while(0 == err)
	{
		err = hori_poll_ir();

		if( 0 == err )
			err = hori_poll_vr_00();

		if( 0 == err )
			err = hori_poll_vr_01();
	}

	hori_shutdown_uinput();
	return 0;
}

