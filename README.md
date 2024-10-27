# mhn_hotas_uinput
Device driver for Mitsubishi Hori/Namco Flightstick 2 HOTAS (Ace Combat 5)

Uses uinput and libusb-1.0

It's developed/tested with Linux 6.10.12

USB ID is 06d3:0f10

This is a userspace implementation of my kernel driver at https://github.com/danieloneill/mhn_hotas

## Credits

Used https://www.tamanegi.org/prog/hfsd/ as reference, and copied the data structs for control messages.

## Notes

* M1/M2/M3 is detected but disabled, as I can't think of a proper way to implement this that doesn't break when binding.
* Neither dial knobs do anything. It turns out they're mouse/kbd URBs, which is a bit more work...
* A and B pressure sensitivity isn't respected. I just trigger "pressed" if they're pressed enough.
* D-PAD1 is split into 4 buttons, D-PAD3 is split into 3 buttons. D-PAD2 is split into 4 buttons.
* HAT +PUSH button doesn't work, but that could be my flight stick? I left the button "on" in the driver in case it works for you.

Keep in mind that for Proton/Wine games Steam likes to emulate an Xbox 360 controller and override HID input devices in the name of compatibility, which will ruin a lot of functionality in this case.

## Usage

```
$ make
cc -o hori hori.c -lusb-1.0

$ sudo ./hori
hori_init_existing: Device registered.
Mitsubishi HORI/Namco Flightstick 2 driver loaded.
hori_poll_ir: Expected 8 bytes, received 4 bytes.
```

Test it with "jstest" or "jstest-gtk"

(Don't mind if the axiseses are a bit confused in the test app, binding them in game works just fine.)

It's safe to ignore that "Expected 8 bytes" warning before the device settles.

If you want to be able to execute it as a regular user (such as on session startup in KDE):

```
$ sudo cp hori /usr/local/bin
$ sudo chmod u+s /usr/local/bin/hori
```
