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
};

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
};

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
};

struct hori_instance {
	struct hori_input_ir	state_ir;
	struct hori_input_vr_00	state_vr00;
	struct hori_input_vr_01	state_vr01;

	int uinp_fd;
	struct uinput_user_dev uinp;

	struct libusb_device *usbdev;
	struct libusb_device_handle *usbdev_handle;

	struct hori_instance *prev, *next;
};

struct hori_instance *hori_handles = NULL;

int hori_uinput_setup_abs(struct hori_instance *inst, int code)
{
	struct uinput_abs_setup setup;
	memset( &setup, 0, sizeof(setup) );
	setup.code = code;
	setup.absinfo.minimum = 0;
	setup.absinfo.maximum = 255;
	setup.absinfo.fuzz = 0;
	setup.absinfo.flat = 0;
	setup.absinfo.resolution = 0;
	setup.absinfo.value = 0;
	ioctl(inst->uinp_fd, UI_ABS_SETUP, &setup);
}

int hori_uinput_init(struct hori_instance *inst)
{
	int err;
	inst->uinp_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
	if(inst->uinp_fd < 0) {
		fprintf(stderr, "hori_uinput_init: open(\"/dev/uinput\") failed: %s\n",
				strerror(inst->uinp_fd));
		return -1;
	}

	err = ioctl(inst->uinp_fd, UI_SET_EVBIT, EV_SYN);
	if(err < 0) {
		fprintf(stderr, "hori_uinput_init: ioctl(%d, UI_SET_EVBIT, EV_SYN) failed: %s\n",
				inst->uinp_fd, strerror(err));
		return -1;
	}

	err = ioctl(inst->uinp_fd, UI_SET_EVBIT, EV_KEY);
	if(err < 0) {
		fprintf(stderr, "hori_uinput_init: ioctl(%d, UI_SET_EVBIT, EV_KEY) failed: %s\n",
				inst->uinp_fd, strerror(err));
		return -1;
	}

	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY1);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY2);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY3);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY4);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY5);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY6);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY7);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER_HAPPY8);

	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TRIGGER);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_THUMB);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_THUMB2);

	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_A);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_B);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_C);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_Y);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_X);

	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_Z);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TL);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_TR);
	ioctl(inst->uinp_fd, UI_SET_KEYBIT, BTN_MODE);

	ioctl(inst->uinp_fd, UI_SET_EVBIT, EV_ABS);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, ABS_X);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, ABS_Y);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, ABS_RUDDER);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, ABS_GAS);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, HORI_HAT_X);
	ioctl(inst->uinp_fd, UI_SET_ABSBIT, HORI_HAT_Y);

	memset(&inst->uinp, 0, sizeof(inst->uinp));
	snprintf(inst->uinp.name, UINPUT_MAX_NAME_SIZE, "Mitsubishi HORI/Namco Flightstick 2");
	inst->uinp.id.bustype = BUS_USB;
	inst->uinp.id.vendor = HORI_VENDOR_ID;
	inst->uinp.id.product = HORI_PRODUCT_ID;
	inst->uinp.id.version = 1;

	err = write(inst->uinp_fd, &inst->uinp, sizeof(inst->uinp));
	if(err < 0) {
		fprintf(stderr, "hori_uinput_init: write(%d, uinp) failed: %s\n",
				inst->uinp_fd, strerror(err));
		return -1;
	}

	hori_uinput_setup_abs(inst, ABS_X);
	hori_uinput_setup_abs(inst, ABS_Y);
	hori_uinput_setup_abs(inst, ABS_RUDDER);
	hori_uinput_setup_abs(inst, ABS_GAS);
	hori_uinput_setup_abs(inst, HORI_HAT_X);
	hori_uinput_setup_abs(inst, HORI_HAT_Y);

	err = ioctl(inst->uinp_fd, UI_DEV_CREATE);
	if(err < 0) {
		fprintf(stderr, "hori_uinput_init: ioctl(%d, UI_DEV_CREATE) failed: %s\n",
				inst->uinp_fd, strerror(err));
		return -1;
	}

	return 0;
}

void hori_uinput_shutdown(struct hori_instance *inst)
{
	if(ioctl(inst->uinp_fd, UI_DEV_DESTROY) < 0) {
		fprintf(stderr, "hori_uinput_shutdown: ioctl(%d, UI_DEV_DESTROY) failed.\n", inst->uinp_fd);
	}
	close(inst->uinp_fd);
}

static inline int hori_uinput_emit(struct hori_instance *inst, int type, int code, int val)
{
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;

	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	int ret = write(inst->uinp_fd, &ie, sizeof(ie));
	if( ret < 0 ) {
		fprintf(stderr, "hori_uinput_emit: write(%d, data, %d) failed.\n",
				inst->uinp_fd, sizeof(ie));
		return -1;
	}
	return 0;
}

struct hori_instance *hori_instance_find(struct libusb_device *usbdev)
{
	struct hori_instance *i;
	for( i=hori_handles; i; i = i->next )
	{
		if( i->usbdev == usbdev )
			return i;
	}

	return NULL;
}

void hori_instance_insert(struct hori_instance *inst)
{
	if( hori_handles )
		hori_handles->prev = inst;

	inst->next = hori_handles;
	hori_handles = inst;
}

void hori_instance_remove(struct hori_instance *inst)
{
	if( inst->prev )
		inst->prev->next = inst->next;
	else
		hori_handles = inst->next;

	if( inst->next )
		inst->next->prev = inst->prev;

	inst->prev = inst->next = NULL;
}

int hori_usb_init(struct hori_instance *inst)
{
	int err = libusb_open(inst->usbdev, &inst->usbdev_handle);
	if(LIBUSB_SUCCESS != err) {
		fprintf(stderr, "hori_usb_init: Device could not be opened. Check that it is connected and that you have appropriate permissions.\n");
		return err;
	}
	
	err = libusb_claim_interface(inst->usbdev_handle, 0);
	if(LIBUSB_SUCCESS != err) {
		fprintf(stderr, "hori_usb_init: Cannot claim interface: %s", libusb_strerror(err));
		return err;
	}

	err = libusb_set_interface_alt_setting(inst->usbdev_handle, 0, 0);
	if(LIBUSB_SUCCESS != err) {
		fprintf(stderr, "hori_usb_init: Cannot set alternate setting: %s", libusb_strerror(err));
		return err;
	}

	return LIBUSB_SUCCESS;
}

struct hori_instance *hori_instance_alloc()
{
	struct hori_instance *inst = (struct hori_instance *)malloc(sizeof(struct hori_instance));

	memset(inst, 0, sizeof(struct hori_instance));

	hori_instance_insert(inst);

	return inst;
}

void hori_instance_free(struct hori_instance *inst)
{
	hori_instance_remove(inst);
	free(inst);
}

void hori_instance_close(struct hori_instance *inst)
{
	struct libusb_device_handle *dev_handle = inst->usbdev_handle;
	hori_uinput_shutdown(inst);
	hori_instance_free(inst);
	printf("hori_usb_hotplug_callback: Device disconnected\n");
	libusb_close(dev_handle);
}

int hori_usb_hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
	struct libusb_device_handle *dev_handle = NULL;
	int rc;
 
	if(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event)
	{
		struct hori_instance *inst = hori_instance_alloc();
		inst->usbdev = dev;
		rc = hori_uinput_init(inst);
		if( 0 != rc)
		{
			fprintf(stderr, "hori_usb_hotplug_callback: Could not create a uinput instance for this device.\n");
			hori_instance_free(inst);
			return 0;
		}

		rc = hori_usb_init(inst);
		if (LIBUSB_SUCCESS != rc) {
			fprintf(stderr, "hori_usb_hotplug_callback: Could not open USB device\n");
			hori_instance_free(inst);
		}

		printf("hori_usb_hotplug_callback: Device connected\n");
	} else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
		// This will usually not happen, because normally we
		// notice the device is DCed during polling.
		struct hori_instance *inst = hori_instance_find(dev);
		if( inst )
			hori_instance_close(inst);
	} else {
		fprintf(stderr, "hori_usb_hotplug_callback: Unhandled event %d\n", event);
	}
 
	return 0;
}

static inline int hori_relbtn_ir(struct hori_instance *inst, int oldval, int newval, int btnid)
{
	if( oldval == newval )
		return 0;

	int oldstate = (oldval > 0xca ? 1 : 0);
	int newstate = (newval > 0xca ? 1 : 0);
	if( oldstate == newstate )
		return 0;

	if( 0 > hori_uinput_emit(inst, EV_KEY, btnid, !newstate) ) {
		fprintf(stderr, "hori_relbtn_ir: hori_uinput_emit failed!\n");
		return -1;
	}

	return 1;
}

#define ABS_IR(FIELD, KEY) if( inst->state_ir.FIELD != ir.FIELD ) { doreport = 1; inst->state_ir.FIELD = ir.FIELD; if( 0 > hori_uinput_emit(inst, EV_ABS, KEY, ir.FIELD) ) { fprintf(stderr, "ABS_IR: hori_uinput_emit failed!\n"); return -1; } }

int hori_poll_ir(struct hori_instance *inst)
{
	int transferred;
	struct hori_input_ir ir;

	int err = libusb_interrupt_transfer(inst->usbdev_handle, 0x81, (unsigned char *)&ir, sizeof(ir), &transferred, 1000);

	// enh.
	if( LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 != err) {
		fprintf(stderr, "hori_poll_ir: libusb_interrupt_transfer failed: %d: %s\n", err, libusb_strerror(err));
		return -1;
	}

	if( transferred != sizeof(ir) ) {
		fprintf(stderr, "hori_poll_ir: Expected %d bytes, received %d bytes.\n",
				sizeof(ir), transferred);
		return 0;
	}

	int64_t *a = (int64_t *)&inst->state_ir;
	int64_t *b = (int64_t *)&ir;
	if( a[0] == b[0] )
		return 0;

	int doreport = 0;
	ABS_IR(pos_x, ABS_X);
	ABS_IR(pos_y, ABS_Y);
	ABS_IR(rudder, ABS_RUDDER);
	ABS_IR(throttle, ABS_GAS);
	ABS_IR(hat_x, HORI_HAT_X);
	ABS_IR(hat_y, HORI_HAT_Y);

	int btn_a_ret = hori_relbtn_ir(inst, inst->state_ir.btn_a, ir.btn_a, BTN_A);
	inst->state_ir.btn_a = ir.btn_a;
	if( btn_a_ret == 1 ) doreport = 1;
	if( btn_a_ret < 0 ) return btn_a_ret;

	int btn_b_ret = hori_relbtn_ir(inst, inst->state_ir.btn_b, ir.btn_b, BTN_B);
	inst->state_ir.btn_b = ir.btn_b;
	if( btn_b_ret == 1 ) doreport = 1;
	if( btn_b_ret < 0 ) return btn_b_ret;

	if( 1 == doreport )
		return hori_uinput_emit(inst, EV_SYN, SYN_REPORT, 0);

	return err;
}

#define KEY_VR00(FIELD, KEY) if( inst->state_vr00.FIELD != vr.FIELD ) { doreport = 1; inst->state_vr00.FIELD = vr.FIELD; if( 0 > hori_uinput_emit(inst, EV_KEY, KEY, !vr.FIELD) ) { fprintf(stderr, "KEY_VR_00: hori_uinput_emit failed!\n"); return -1; } }
int hori_poll_vr_00(struct hori_instance *inst)
{
	struct hori_input_vr_00 vr;

	int err = libusb_control_transfer(inst->usbdev_handle,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_ENDPOINT,
			HORI_POLL_VR0, 0, 1, (unsigned char *)&vr, sizeof(vr), 200);

	// Handle stall. (Do nothing, we're in a control xfer.)
	if( LIBUSB_ERROR_PIPE == err || LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 > err) {
		fprintf(stderr, "hori_poll_vr_00: libusb_control_transfer failed: %d: %s\n", err, libusb_strerror(err));
		return -1;
	}

	if( err != sizeof(vr) ) {
		fprintf(stderr, "hori_poll_vr_00: Expected %d bytes, received %d bytes.\n",
				sizeof(vr), err);
		return 0;
	}

	int16_t *a = (int16_t *)&inst->state_vr00;
	int16_t *b = (int16_t *)&vr;
	if( a[0] == b[0] )
		return 0;

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
		return hori_uinput_emit(inst, EV_SYN, SYN_REPORT, 0);

	return 0;
}

#define KEY_VR01(FIELD, KEY) if( inst->state_vr01.FIELD != vr.FIELD ) { doreport = 1; inst->state_vr01.FIELD = vr.FIELD; if( 0 > hori_uinput_emit(inst, EV_KEY, KEY, !vr.FIELD) ) { fprintf(stderr, "KEY_VR_01: hori_uinput_emit failed!\n"); return -1; } }
int hori_poll_vr_01(struct hori_instance *inst)
{
	struct hori_input_vr_01 vr;

	int err = libusb_control_transfer(inst->usbdev_handle,
			LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_ENDPOINT,
			HORI_POLL_VR1, 0, 1, (unsigned char *)&vr, sizeof(vr), 200);

	// Handle stall. (Do nothing, we're in a control xfer.)
	if( LIBUSB_ERROR_PIPE == err || LIBUSB_ERROR_TIMEOUT == err )
		return 0;

	if( 0 > err) {
		fprintf(stderr, "hori_poll_vr_01: libusb_control_transfer failed: %d: %s\n", err, libusb_strerror(err));
		return -1;
	}

	if( err != sizeof(vr) ) {
		fprintf(stderr, "hori_poll_vr_01: Expected %d bytes, received %d bytes.\n",
				sizeof(vr), err);
		return 0;
	}

	int16_t *a = (int16_t *)&inst->state_vr01;
	int16_t *b = (int16_t *)&vr;
	if( a[0] == b[0] )
		return 0;

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
		return hori_uinput_emit(inst, EV_SYN, SYN_REPORT, 0);

	return 0;
}

int hori_poll()
{
	int err = 0;
	struct hori_instance *i, *n;
	for( i=hori_handles; i; i=n)
	{
		n = i->next;
		err = hori_poll_ir(i);
	
		if( 0 == err )
			err = hori_poll_vr_00(i);
	
		if( 0 == err )
			err = hori_poll_vr_01(i);

		if( 0 != err )
			hori_instance_close(i);
	}

	return err;
}

int hori_init_existing()
{
	libusb_device **devlist = NULL;
	ssize_t count = libusb_get_device_list(NULL, &devlist);
	for(ssize_t i = 0; i < count; i++)
	{
		struct libusb_device_descriptor desc;
		libusb_device *device = devlist[i];

		int ret = libusb_get_device_descriptor(device, &desc);
		if( LIBUSB_SUCCESS != ret )
			continue;

		if( HORI_VENDOR_ID != desc.idVendor )
			continue;

		if( HORI_PRODUCT_ID != desc.idProduct )
			continue;

		struct hori_instance *inst = hori_instance_alloc();
		inst->usbdev = device;
		ret = hori_uinput_init(inst);
		if( 0 != ret)
		{
			fprintf(stderr, "hori_init_existing: Could not create a uinput instance for this device.\n");
			hori_instance_free(inst);
			continue;
		}

		ret = hori_usb_init(inst);
		if (LIBUSB_SUCCESS != ret) {
			fprintf(stderr, "hori_init_existing: Could not open USB device\n");
			hori_instance_free(inst);
		}
		fprintf(stderr, "hori_init_existing: Device registered.\n");
	}
	libusb_free_device_list(devlist, 1);
}

int main(int argc, char **argv)
{
	int ret;
	libusb_hotplug_callback_handle callback_handle;

	libusb_init(NULL);

	//libusb_init_context(NULL, NULL, 0);

	ret = libusb_hotplug_register_callback(NULL,
			LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
			0,
			HORI_VENDOR_ID,
			HORI_PRODUCT_ID,
			LIBUSB_HOTPLUG_MATCH_ANY,
			hori_usb_hotplug_callback,
			NULL,
			&callback_handle);

	if (LIBUSB_SUCCESS != ret) {
		printf("Error creating a hotplug callback\n");
		libusb_exit(NULL);
		return EXIT_FAILURE;
	}

	// Try to pick up any devices currently connected.
	hori_init_existing();

	printf("Mitsubishi HORI/Namco Flightstick 2 driver loaded.\n");

	struct timespec ts;
	struct timeval tv;
	uint32_t chillout = 0;
	while (1) {
		hori_poll();

		// effectively limit polling to 5ms intervals
		// to reduce cpu usage:
		tv.tv_sec = 0;
		tv.tv_usec = 5000;

		// only check hotplug events every 5 seconds
		// to reduce cpu usage:
		if( chillout++ % 1000 == 0 ) {
			// if no devices, block for a full minute"
			if( NULL == hori_handles )
				tv.tv_sec = 60;

			libusb_handle_events_timeout_completed(NULL, &tv, NULL);
		} else
			select(0, NULL, NULL, NULL, &tv);
	}

	libusb_hotplug_deregister_callback(NULL, callback_handle);
	libusb_exit(NULL);

	return 0;
}

