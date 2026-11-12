#ifdef HW_RVL
#include <gccore.h>
#include <string.h>

#define RETROUSB_VID 61440
#define RETROUSB_PID 8	

#define POLL_THREAD_STACKSIZE		(1024 * 4)
#define POLL_THREAD_PRIO			65

static bool setup = false;
static bool replugRequired = false;
static s32 deviceId = 0;
static u8 endpoint = 0;
static u8 bMaxPacketSize = 0;
static u32 jpRetroUSB;
static char testChars[65];
static bool pollThreadRunning = false;
static lwp_t pollThread = LWP_THREAD_NULL;
static u8 pollStack[POLL_THREAD_STACKSIZE] ATTRIBUTE_ALIGN(8);
u8 RetroUSB_Counter = 0;

static bool isRetroUSBGamepad(usb_device_entry dev)
{
	return dev.vid == RETROUSB_VID && dev.pid == RETROUSB_PID;
}

static u8 getEndpoint(usb_devdesc devdesc)
{
	if (devdesc.configurations == NULL || devdesc.configurations->interfaces == NULL ||
		devdesc.configurations->interfaces->endpoints == NULL)
	{
		return -1;
	}
	return devdesc.configurations->interfaces->endpoints->bEndpointAddress;
}

static int removal_cb(int result, void* usrdata)
{
	s32 fd = (s32)usrdata;
	if (fd == deviceId)
	{
		deviceId = 0;
	}
	return 1;
}

static void open()
{
	if (deviceId != 0)
	{
		return;
	}

	usb_device_entry dev_entry[8];
	u8 dev_count;
	if (USB_GetDeviceList(dev_entry, 8, USB_CLASS_HID, &dev_count) < 0)
	{
		return;
	}

	for (int i = 0; i < dev_count; ++i)
	{
		if (!isRetroUSBGamepad(dev_entry[i]))
		{
			continue;
		}
		s32 fd;
		if (USB_OpenDevice(dev_entry[i].device_id, dev_entry[i].vid, dev_entry[i].pid, &fd) < 0)
		{
			continue;
		}

		usb_devdesc devdesc;
		if (USB_GetDescriptors(fd, &devdesc) < 0)
		{
			// You have to replug the controller!
			replugRequired = true;
			USB_CloseDevice(&fd);
			break;
		}

		deviceId = fd;
		replugRequired = false;
		endpoint = getEndpoint(devdesc);
		bMaxPacketSize = devdesc.bMaxPacketSize0;
		USB_DeviceRemovalNotifyAsync(fd, &removal_cb, (void*)fd);
		break;
	}

	setup = true;
}

u32 RetroUSB_ScanPads(void)
{
	return jpRetroUSB;
}

static void *scanThreadFunc(void *arg)
{
	RetroUSB_Counter = 100;
	while (1)
	{
		if (deviceId == 0 || replugRequired)
		{
			continue;
		}

		uint8_t ATTRIBUTE_ALIGN(32) buf[4];
		s32 res = USB_ReadIntrMsg(deviceId, endpoint, sizeof(buf), buf);
		if (res < 0)
		{
			continue;
		}

		u32 jp = 0;
		jp |= (buf[2] & 0x80) ? PAD_BUTTON_UP : 0;
		jp |= (buf[2] & 0x40) ? PAD_BUTTON_DOWN : 0;
		jp |= (buf[2] & 0x20) ? PAD_BUTTON_LEFT : 0;
		jp |= (buf[2] & 0x10) ? PAD_BUTTON_RIGHT : 0;

		jp |= ((buf[2] & 0x08)) ? PAD_BUTTON_A : 0;
		jp |= ((buf[2] & 0x04)) ? PAD_BUTTON_B : 0;
		jp |= ((buf[3] & 0x04)) ? PAD_BUTTON_X : 0;
		jp |= ((buf[2] & 0x02)) ? PAD_BUTTON_Y : 0;

		jp |= ((buf[3] & 0x02)) ? PAD_TRIGGER_L : 0;
		jp |= ((buf[3] & 0x08)) ? PAD_TRIGGER_R : 0;

		jp |= ((buf[2] & 0x01)) ? PAD_BUTTON_START : 0;
		jp |= ((buf[3] & 0x01)) ? PAD_TRIGGER_Z : 0;

		/*int i;
		for (i = 0; i < res; i++) {
			testChars[i * 3] = "0123456789ABCDEF"[(buf[i] & 0xF0) >> 4];
			testChars[i * 3 + 1] = "0123456789ABCDEF"[buf[i] & 0xF];
			testChars[i * 3 + 2] = ' ';
		}

		counter++;
		testChars[i * 3] = "0123456789ABCDEF"[(counter & 0xF0) >> 4];
		testChars[i * 3 + 1] = "0123456789ABCDEF"[counter & 0xF];
		testChars[i * 3 + 2] = '\0';*/

		RetroUSB_Counter++;

		jpRetroUSB = jp;
	}

	return 0;
}

u32 RetroUSB_ButtonsHeld()
{
	if (!setup)
	{
		open();
	}
	if (!pollThreadRunning)
	{
		memset(pollStack, 0, POLL_THREAD_STACKSIZE);

		s32 res = LWP_CreateThread(&pollThread, scanThreadFunc, NULL,
			pollStack, POLL_THREAD_STACKSIZE,
			POLL_THREAD_PRIO);
		if (!res)
		{
			pollThreadRunning = true;
		}
	}
	if (deviceId == 0)
	{
		return 0;
	}
	return jpRetroUSB;
}

char* RetroUSB_TestChars() {
	return testChars;
}

bool RetroUSB_OK()
{
	open();
	return !replugRequired && deviceId;
}

#endif