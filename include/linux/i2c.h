/* ------------------------------------------------------------------------- */
/*									     */
/* i2c.h - definitions for the i2c-bus interface			     */
/*									     */
/* ------------------------------------------------------------------------- */
/*   Copyright (C) 1995-2000 Simon G. Vogl

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
    MA 02110-1301 USA.							     */
/* ------------------------------------------------------------------------- */

/* With some changes from Kyösti Mälkki <kmalkki@cc.hut.fi> and
   Frodo Looijaard <frodol@dds.nl> */
#ifndef _LINUX_I2C_H
#define _LINUX_I2C_H

#include <linux/mod_devicetable.h>
#include <linux/device.h>	/* for struct device */
#include <linux/sched.h>	/* for completion */
#include <linux/mutex.h>
#include <linux/of.h>		/* for struct device_node */
#include <linux/swab.h>		/* for swab16 */
#include <uapi/linux/i2c.h>
#include <linux/hrtimer.h>

extern struct bus_type i2c_bus_type;
extern struct device_type i2c_adapter_type;

/* --- General options ------------------------------------------------	*/

struct i2c_msg;
struct i2c_algorithm;
struct i2c_adapter;
struct i2c_client;
struct i2c_driver;
union i2c_smbus_data;
struct i2c_board_info;
struct i2c_op_q_entry;

struct module;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 * The master routines are the ones normally used to transmit data to devices
 * on a bus (or read from them). Apart from two basic transfer functions to
 * transmit one message at a time, a more complex version can be used to
 * transmit an arbitrary number of messages without interruption.
 * @count must be be less than 64k since msg.len is u16.
 */
extern int i2c_master_send(const struct i2c_client *client, const char *buf,
			   int count);
extern int i2c_master_recv(const struct i2c_client *client, char *buf,
			   int count);

/* Transfer num messages.
 */
extern int i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			int num);
/* Unlocked flavor */
extern int __i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs,
			  int num);

/* This is the very generalized SMBus access routine. You probably do not
   want to use this, though; one of the functions below may be much easier,
   and probably just as fast.
   Note that we use i2c_adapter here, because you do not need a specific
   smbus adapter to call this function. */
extern s32 i2c_smbus_xfer(struct i2c_adapter *adapter, u16 addr,
			  unsigned short flags, char read_write, u8 command,
			  int size, union i2c_smbus_data *data);

/* Now follow the 'nice' access routines. These also document the calling
   conventions of i2c_smbus_xfer. */

extern s32 i2c_smbus_read_byte(const struct i2c_client *client);
extern s32 i2c_smbus_write_byte(const struct i2c_client *client, u8 value);
extern s32 i2c_smbus_read_byte_data(const struct i2c_client *client,
				    u8 command);
extern s32 i2c_smbus_write_byte_data(const struct i2c_client *client,
				     u8 command, u8 value);
extern s32 i2c_smbus_read_word_data(const struct i2c_client *client,
				    u8 command);
extern s32 i2c_smbus_write_word_data(const struct i2c_client *client,
				     u8 command, u16 value);

static inline s32
i2c_smbus_read_word_swapped(const struct i2c_client *client, u8 command)
{
	s32 value = i2c_smbus_read_word_data(client, command);

	return (value < 0) ? value : swab16(value);
}

static inline s32
i2c_smbus_write_word_swapped(const struct i2c_client *client,
			     u8 command, u16 value)
{
	return i2c_smbus_write_word_data(client, command, swab16(value));
}

/* Returns the number of read bytes */
extern s32 i2c_smbus_read_block_data(const struct i2c_client *client,
				     u8 command, u8 *values);
extern s32 i2c_smbus_write_block_data(const struct i2c_client *client,
				      u8 command, u8 length, const u8 *values);
/* Returns the number of read bytes */
extern s32 i2c_smbus_read_i2c_block_data(const struct i2c_client *client,
					 u8 command, u8 length, u8 *values);
extern s32 i2c_smbus_write_i2c_block_data(const struct i2c_client *client,
					  u8 command, u8 length,
					  const u8 *values);
#endif /* I2C */

/*
 * Non-blocking interface.  The user should fill out the public
 * portions of the entry structure.  All data in the entry structure
 * should be guaranteed to be available until the handler callback is
 * called with the entry.
 */
extern int i2c_non_blocking_op(struct i2c_client *client,
			       struct i2c_op_q_entry *entry);

/*
 * Poll the i2c interface.  This should only be called in a situation
 * where scheduling and interrupts are off.  You should put the amount
 * of nanoseconds between calls in ns_since_last_call.
 */
extern void i2c_poll(struct i2c_client *client,
		     unsigned int ns_since_last_call);

/**
 * struct i2c_driver - represent an I2C device driver
 * @class: What kind of i2c device we instantiate (for detect)
 * @attach_adapter: Callback for bus addition (deprecated)
 * @probe: Callback for device binding
 * @remove: Callback for device unbinding
 * @shutdown: Callback for device shutdown
 * @suspend: Callback for device suspend
 * @resume: Callback for device resume
 * @alert: Alert callback, for example for the SMBus alert protocol
 * @command: Callback for bus-wide signaling (optional)
 * @driver: Device driver model driver
 * @id_table: List of I2C devices supported by this driver
 * @detect: Callback for device detection
 * @address_list: The I2C addresses to probe (for detect)
 * @clients: List of detected clients we created (for i2c-core use only)
 *
 * The driver.owner field should be set to the module owner of this driver.
 * The driver.name field should be set to the name of this driver.
 *
 * For automatic device detection, both @detect and @address_list must
 * be defined. @class should also be set, otherwise only devices forced
 * with module parameters will be created. The detect function must
 * fill at least the name field of the i2c_board_info structure it is
 * handed upon successful detection, and possibly also the flags field.
 *
 * If @detect is missing, the driver will still work fine for enumerated
 * devices. Detected devices simply won't be supported. This is expected
 * for the many I2C/SMBus devices which can't be detected reliably, and
 * the ones which can always be enumerated in practice.
 *
 * The i2c_client structure which is handed to the @detect callback is
 * not a real i2c_client. It is initialized just enough so that you can
 * call i2c_smbus_read_byte_data and friends on it. Don't do anything
 * else with it. In particular, calling dev_dbg and friends on it is
 * not allowed.
 */
struct i2c_driver {
	unsigned int class;

	/* Notifies the driver that a new bus has appeared. You should avoid
	 * using this, it will be removed in a near future.
	 */
	int (*attach_adapter)(struct i2c_adapter *) __deprecated;

	/* Standard driver model interfaces */
	int (*probe)(struct i2c_client *, const struct i2c_device_id *);
	int (*remove)(struct i2c_client *);

	/* driver model interfaces that don't relate to enumeration  */
	void (*shutdown)(struct i2c_client *);
	int (*suspend)(struct i2c_client *, pm_message_t mesg);
	int (*resume)(struct i2c_client *);

	/* Alert callback, for example for the SMBus alert protocol.
	 * The format and meaning of the data value depends on the protocol.
	 * For the SMBus alert protocol, there is a single bit of data passed
	 * as the alert response's low bit ("event flag").
	 */
	void (*alert)(struct i2c_client *, unsigned int data);

	/* a ioctl like command that can be used to perform specific functions
	 * with the device.
	 */
	int (*command)(struct i2c_client *client, unsigned int cmd, void *arg);

	struct device_driver driver;
	const struct i2c_device_id *id_table;

	/* Device detection callback for automatic device creation */
	int (*detect)(struct i2c_client *, struct i2c_board_info *);
	const unsigned short *address_list;
	struct list_head clients;
};
#define to_i2c_driver(d) container_of(d, struct i2c_driver, driver)

/**
 * struct i2c_client - represent an I2C slave device
 * @flags: I2C_CLIENT_TEN indicates the device uses a ten bit chip address;
 *	I2C_CLIENT_PEC indicates it uses SMBus Packet Error Checking
 * @addr: Address used on the I2C bus connected to the parent adapter.
 * @name: Indicates the type of the device, usually a chip name that's
 *	generic enough to hide second-sourcing and compatible revisions.
 * @adapter: manages the bus segment hosting this I2C device
 * @driver: device's driver, hence pointer to access routines
 * @dev: Driver model device node for the slave.
 * @irq: indicates the IRQ generated by this device (if any)
 * @detected: member of an i2c_driver.clients list or i2c-core's
 *	userspace_devices list
 *
 * An i2c_client identifies a single device (i.e. chip) connected to an
 * i2c bus. The behaviour exposed to Linux is defined by the driver
 * managing the device.
 */
struct i2c_client {
	unsigned short flags;		/* div., see below		*/
	unsigned short addr;		/* chip address - NOTE: 7bit	*/
					/* addresses are stored in the	*/
					/* _LOWER_ 7 bits		*/
	char name[I2C_NAME_SIZE];
	struct i2c_adapter *adapter;	/* the adapter we sit on	*/
	struct i2c_driver *driver;	/* and our access routines	*/
	struct device dev;		/* the device structure		*/
	char *parms;			/* sysfs extra parms		*/
	int irq;			/* irq issued by device		*/
	struct list_head detected;
};
#define to_i2c_client(d) container_of(d, struct i2c_client, dev)

extern struct i2c_client *i2c_verify_client(struct device *dev);
extern struct i2c_adapter *i2c_verify_adapter(struct device *dev);

static inline struct i2c_client *kobj_to_i2c_client(struct kobject *kobj)
{
	struct device * const dev = container_of(kobj, struct device, kobj);
	return to_i2c_client(dev);
}

static inline void *i2c_get_clientdata(const struct i2c_client *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void i2c_set_clientdata(struct i2c_client *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

/**
 * struct i2c_board_info - template for device creation
 * @type: chip type, to initialize i2c_client.name
 * @flags: to initialize i2c_client.flags
 * @addr: stored in i2c_client.addr
 * @platform_data: stored in i2c_client.dev.platform_data
 * @archdata: copied into i2c_client.dev.archdata
 * @of_node: pointer to OpenFirmware device node
 * @acpi_node: ACPI device node
 * @parms: Parameters supplied on the sysfs command line
 * @irq: stored in i2c_client.irq
 *
 * I2C doesn't actually support hardware probing, although controllers and
 * devices may be able to use I2C_SMBUS_QUICK to tell whether or not there's
 * a device at a given address.  Drivers commonly need more information than
 * that, such as chip type, configuration, associated IRQ, and so on.
 *
 * i2c_board_info is used to build tables of information listing I2C devices
 * that are present.  This information is used to grow the driver model tree.
 * For mainboards this is done statically using i2c_register_board_info();
 * bus numbers identify adapters that aren't yet available.  For add-on boards,
 * i2c_new_device() does this dynamically with the adapter already known.
 */
struct i2c_board_info {
	char		type[I2C_NAME_SIZE];
	unsigned short	flags;
	unsigned short	addr;
	void		*platform_data;
	struct dev_archdata	*archdata;
	struct device_node *of_node;
	struct acpi_dev_node acpi_node;
	char            *parms;
	int		irq;
};

/**
 * I2C_BOARD_INFO - macro used to list an i2c device and its address
 * @dev_type: identifies the device type
 * @dev_addr: the device's address on the bus.
 *
 * This macro initializes essential fields of a struct i2c_board_info,
 * declaring what has been provided on a particular board.  Optional
 * fields (such as associated irq, or device-specific platform_data)
 * are provided using conventional syntax.
 */
#define I2C_BOARD_INFO(dev_type, dev_addr) \
	.type = dev_type, .addr = (dev_addr)


#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/* Add-on boards should register/unregister their devices; e.g. a board
 * with integrated I2C, a config eeprom, sensors, and a codec that's
 * used in conjunction with the primary hardware.
 */
extern struct i2c_client *
i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info);

/* If you don't know the exact address of an I2C device, use this variant
 * instead, which can probe for device presence in a list of possible
 * addresses. The "probe" callback function is optional. If it is provided,
 * it must return 1 on successful probe, 0 otherwise. If it is not provided,
 * a default probing method is used.
 */
extern struct i2c_client *
i2c_new_probed_device(struct i2c_adapter *adap,
		      struct i2c_board_info *info,
		      unsigned short const *addr_list,
		      int (*probe)(struct i2c_adapter *, unsigned short addr));

/* Common custom probe functions */
extern int i2c_probe_func_quick_read(struct i2c_adapter *, unsigned short addr);

/* For devices that use several addresses, use i2c_new_dummy() to make
 * client handles for the extra addresses.
 */
extern struct i2c_client *
i2c_new_dummy(struct i2c_adapter *adap, u16 address);

extern void i2c_unregister_device(struct i2c_client *);
#endif /* I2C */

/* Mainboard arch_initcall() code should register all its I2C devices.
 * This is done at arch_initcall time, before declaring any i2c adapters.
 * Modules for add-on boards must use other calls.
 */
#ifdef CONFIG_I2C_BOARDINFO
extern int
i2c_register_board_info(int busnum, struct i2c_board_info const *info,
			unsigned n);
#else
static inline int
i2c_register_board_info(int busnum, struct i2c_board_info const *info,
			unsigned n)
{
	return 0;
}
#endif /* I2C_BOARDINFO */

/*
 * About locking and the non-blocking interface.
 *
 * The poll operations are called single-threaded (along with the
 * xxx_start operations), so if the driver is only polled then there
 * is no need to do any locking.  If you are using interrupts, then
 * the timer operations and interrupts can race and you need to lock
 * appropriately.
 *
 * i2c_op_done() can be called multiple times on the same entry (as
 * long as each one has a get operation).  This handles poll and
 * interrupt races calling i2c_op_done().  It will do the right thing.
 */

/*
 * Called from an non-blocking interface to get the current working
 * entry.  Returns NULL if there is none.  This is primarily for
 * interrupt handlers to determine what they should be working on.
 * Note that if you call i2c_entry_get() and get a non-null entry, you
 * must call i2c_entry_put() on it.
 */
struct i2c_op_q_entry *i2c_entry_get(struct i2c_adapter *adap);
void i2c_entry_put(struct i2c_adapter *adap,
		   struct i2c_op_q_entry *entry);

/*
 * Called from an non-blocking interface to report that an operation
 * has completed.  Can be called from interrupt context.
 */
void i2c_op_done(struct i2c_adapter *adap, struct i2c_op_q_entry *entry);

/*
 * The following structs are for those who like to implement new bus drivers:
 * i2c_algorithm is the interface to a class of hardware solutions which can
 * be addressed using the same bus algorithms - i.e. bit-banging or the PCF8584
 * to name two of the most common.
 */
struct i2c_algorithm {
	/* If an adapter algorithm can't do I2C-level access, set master_xfer
	   to NULL. If an adapter algorithm can do SMBus access, set
	   smbus_xfer. If set to NULL, the SMBus protocol is simulated
	   using common I2C messages */
	/* master_xfer should return the number of messages successfully
	   processed, or a negative value on error */
	int (*master_xfer)(struct i2c_adapter *adap, struct i2c_msg *msgs,
			   int num);
	int (*smbus_xfer) (struct i2c_adapter *adap, u16 addr,
			   unsigned short flags, char read_write,
			   u8 command, int size, union i2c_smbus_data *data);

	/*
	 * These are like the previous calls, but they will only start
	 * the operation.  The poll call will be called periodically
	 * to drive the operation of the bus.  Each of these calls
	 * should set the result on an error, and set the timeout as
	 * necessary.  Note that even interrupt driven drivers need to
	 * poll so they can time out operations.  Note that all the
	 * data structures passed in are guaranteed to be kept around
	 * until the operation completes.  These may be called from
	 * interrupt context.  If the start operation fails, these
	 * should return an error.  They are called with the queue lock
	 * held, so they should not call i2c_op_done().
	 */
	int (*master_start)(struct i2c_adapter    *adap,
			    struct i2c_op_q_entry *entry);
	int (*smbus_start)(struct i2c_adapter    *adap,
			   struct i2c_op_q_entry *entry);
	/*
	 * ns_since_last_poll is the amount of time since the last
	 * time poll was called. Note that this may be *less* than the
	 * time you requested, so always use this number and don't
	 * assume it's the one you gave it.  This time is approximate
	 * and is only guaranteed to be >= the time since the last
	 * poll.  The value may be zero.
	 */
	void (*poll)(struct i2c_adapter *adap,
		     struct i2c_op_q_entry *entry,
		     unsigned int ns_since_last_poll);

	/* To determine what the adapter supports */
	u32 (*functionality) (struct i2c_adapter *);
};
typedef struct i2c_algorithm __no_const i2c_algorithm_no_const;

/**
 * struct i2c_bus_recovery_info - I2C bus recovery information
 * @recover_bus: Recover routine. Either pass driver's recover_bus() routine, or
 *	i2c_generic_scl_recovery() or i2c_generic_gpio_recovery().
 * @get_scl: This gets current value of SCL line. Mandatory for generic SCL
 *      recovery. Used internally for generic GPIO recovery.
 * @set_scl: This sets/clears SCL line. Mandatory for generic SCL recovery. Used
 *      internally for generic GPIO recovery.
 * @get_sda: This gets current value of SDA line. Optional for generic SCL
 *      recovery. Used internally, if sda_gpio is a valid GPIO, for generic GPIO
 *      recovery.
 * @prepare_recovery: This will be called before starting recovery. Platform may
 *	configure padmux here for SDA/SCL line or something else they want.
 * @unprepare_recovery: This will be called after completing recovery. Platform
 *	may configure padmux here for SDA/SCL line or something else they want.
 * @scl_gpio: gpio number of the SCL line. Only required for GPIO recovery.
 * @sda_gpio: gpio number of the SDA line. Only required for GPIO recovery.
 */
struct i2c_bus_recovery_info {
	int (*recover_bus)(struct i2c_adapter *);

	int (*get_scl)(struct i2c_adapter *);
	void (*set_scl)(struct i2c_adapter *, int val);
	int (*get_sda)(struct i2c_adapter *);

	void (*prepare_recovery)(struct i2c_bus_recovery_info *bri);
	void (*unprepare_recovery)(struct i2c_bus_recovery_info *bri);

	/* gpio recovery */
	int scl_gpio;
	int sda_gpio;
};

int i2c_recover_bus(struct i2c_adapter *adap);

/* Generic recovery routines */
int i2c_generic_gpio_recovery(struct i2c_adapter *adap);
int i2c_generic_scl_recovery(struct i2c_adapter *adap);

/*
 * i2c_adapter is the structure used to identify a physical i2c bus along
 * with the access algorithms necessary to access it.
 */
struct i2c_adapter {
	struct module *owner;
	unsigned int class;		  /* classes to allow probing for */
	const struct i2c_algorithm *algo; /* the algorithm to access the bus */
	void *algo_data;

	/* data fields that are valid for all devices	*/
	struct rt_mutex bus_lock;

	struct list_head q;
	spinlock_t q_lock;

	struct hrtimer timer;

	int timeout;			/* in jiffies */
	int retries;
	struct device dev;		/* the adapter device */

	int nr;
	char name[48];
	struct completion dev_released;

	struct mutex userspace_clients_lock;
	struct list_head userspace_clients;

	struct i2c_bus_recovery_info *bus_recovery_info;
};
#define to_i2c_adapter(d) container_of(d, struct i2c_adapter, dev)

static inline void *i2c_get_adapdata(const struct i2c_adapter *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void i2c_set_adapdata(struct i2c_adapter *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline struct i2c_adapter *
i2c_parent_is_i2c_adapter(const struct i2c_adapter *adapter)
{
	struct device *parent = adapter->dev.parent;

	if (parent != NULL && parent->type == &i2c_adapter_type)
		return to_i2c_adapter(parent);
	else
		return NULL;
}

int i2c_for_each_dev(void *data, int (*fn)(struct device *, void *));

/* Adapter locking functions, exported for shared pin cases */
void i2c_lock_adapter(struct i2c_adapter *);
void i2c_unlock_adapter(struct i2c_adapter *);

/*flags for the client struct: */
#define I2C_CLIENT_PEC	0x04		/* Use Packet Error Checking */
#define I2C_CLIENT_TEN	0x10		/* we have a ten bit chip address */
					/* Must equal I2C_M_TEN below */
#define I2C_CLIENT_WAKE	0x80		/* for board_info; true iff can wake */
#define I2C_CLIENT_SCCB	0x9000		/* Use Omnivision SCCB protocol */
					/* Must match I2C_M_STOP|IGNORE_NAK */

/* i2c adapter classes (bitmask) */
#define I2C_CLASS_HWMON		(1<<0)	/* lm_sensors, ... */
#define I2C_CLASS_DDC		(1<<3)	/* DDC bus on graphics adapters */
#define I2C_CLASS_SPD		(1<<7)	/* Memory modules */

/* Internal numbers to terminate lists */
#define I2C_CLIENT_END		0xfffeU

/* Construct an I2C_CLIENT_END-terminated array of i2c addresses */
#define I2C_ADDRS(addr, addrs...) \
	((const unsigned short []){ addr, ## addrs, I2C_CLIENT_END })


/* ----- functions exported by i2c.o */

/* administration...
 */
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
extern int i2c_add_adapter(struct i2c_adapter *);
extern void i2c_del_adapter(struct i2c_adapter *);
extern int i2c_add_numbered_adapter(struct i2c_adapter *);

extern int i2c_register_driver(struct module *, struct i2c_driver *);
extern void i2c_del_driver(struct i2c_driver *);

/* use a define to avoid include chaining to get THIS_MODULE */
#define i2c_add_driver(driver) \
	i2c_register_driver(THIS_MODULE, driver)

extern struct i2c_client *i2c_use_client(struct i2c_client *client);
extern void i2c_release_client(struct i2c_client *client);

/* call the i2c_client->command() of all attached clients with
 * the given arguments */
extern void i2c_clients_command(struct i2c_adapter *adap,
				unsigned int cmd, void *arg);

extern struct i2c_adapter *i2c_get_adapter(int nr);
extern void i2c_put_adapter(struct i2c_adapter *adap);


/* Return the functionality mask */
static inline u32 i2c_get_functionality(struct i2c_adapter *adap)
{
	return adap->algo->functionality(adap);
}

/* Return 1 if adapter supports everything we need, 0 if not. */
static inline int i2c_check_functionality(struct i2c_adapter *adap, u32 func)
{
	return (func & i2c_get_functionality(adap)) == func;
}

/* Return the adapter number for a specific adapter */
static inline int i2c_adapter_id(struct i2c_adapter *adap)
{
	return adap->nr;
}

/* Is the interface capable of using the non-blocking interface? */
static inline int i2c_non_blocking_capable(struct i2c_adapter *adap)
{
	return adap->algo->poll != NULL;
}

/**
 * module_i2c_driver() - Helper macro for registering a I2C driver
 * @__i2c_driver: i2c_driver struct
 *
 * Helper macro for I2C drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate. Each module may only
 * use this macro once, and calling it replaces module_init() and module_exit()
 */
#define module_i2c_driver(__i2c_driver) \
	module_driver(__i2c_driver, i2c_add_driver, \
			i2c_del_driver)

#endif /* I2C */

#if IS_ENABLED(CONFIG_ACPI_I2C)
extern void acpi_i2c_register_devices(struct i2c_adapter *adap);
#else
static inline void acpi_i2c_register_devices(struct i2c_adapter *adap) {}
#endif

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 * Hold an I2C operation to perform, and used to pass data around.
 */
#define I2C_OP_I2C	0
#define I2C_OP_SMBUS	1
typedef void (*i2c_op_done_cb)(struct i2c_op_q_entry *entry);
struct i2c_op_q_entry {
	/*
	 * The result will be set to the result of the operation when
	 * it completes.
	 */
	s32 result;

	/**************************************************************/
	/*
	 * Public interface.  The user should set these up (and the
	 * proper structure below).
	 */

	/*
	 * Set to I2C_OP_I2C or I2C_OP_SMBUS depending on the transfer type.
	 */
	int            xfer_type;

	/*
	 * Handler may be called from interrupt context, so be
	 * careful.
	 */
	i2c_op_done_cb handler;
	void           *handler_data;

	/*
	 * Set up the i2c or smbus structure, depending on the transfer
	 * type.
	 *
	 * Note that i2c and smbus are not in a union because an smbus
	 * operation may be converted into an i2c operation (thus both
	 * structures will be used).  The data in these may be changed
	 * by the driver.
	 */
	struct {
		struct i2c_msg *msgs;
		int num;
	} i2c;
	struct {
		/*
		 * Addr and flags are filled in by the non-blocking
		 * send routine that takes a client.
		 */
		u16 addr;
		unsigned short flags;

		char read_write;
		u8 command;

		/*
		 * Note that the size is *not* the length of the data.
		 * It is the transaction type, like I2C_SMBUS_QUICK
		 * and the ones after that below.  If this is a block
		 * transaction, the length of the rest of the data is
		 * in the first byte of the data, for both transmit
		 * and receive.
		 */
		int size;
		union i2c_smbus_data *data;
	} smbus;

	/**************************************************************/
	/* Bus Interface */
	/*
	 * The bus interface must set call_again_ns to the time in
	 * nanoseconds until the next poll operation should be
	 * called.  This *must* be set in the start operation
	 * function.  The value may be changed in poll calls if the
	 * bus interface needs different timeouts at different times.
	 * The time_left and data can be used for anything the bus
	 * interface likes.  data will be set to NULL before being
	 * started; the bus interface must use that to tell if the
	 * entry has been set up.  It should ignore poll operations on
	 * entries that are not yet set up.
	 */
	unsigned long call_again_ns;
	long          time_left;
	void	      *data;

	/**************************************************************/
	/* Internals */
	struct completion done;
	unsigned long     end_jiffies;
	unsigned int      tries;
	unsigned char     use_timer;
  
#define I2C_OP_QUEUED		0
#define I2C_OP_INITIALIZED	1
#define I2C_OP_FINISHED		2
	unsigned char	  state;
	u8                pec;
	u8                partial_pec;
	void (*complete)(struct i2c_adapter    *adap,
			 struct i2c_op_q_entry *entry);

	struct list_head link;
	struct kref usecount;

	/*
	 * These are here for SMBus emulation over I2C.  I don't like
	 * them taking this much room in the data structure, but they
	 * need to be available in this case.
	 */
	unsigned char msgbuf0[I2C_SMBUS_BLOCK_MAX + 3];
	unsigned char msgbuf1[I2C_SMBUS_BLOCK_MAX + 2];
	struct i2c_msg msg[2];
};

#endif /* I2C */
#endif /* _LINUX_I2C_H */