#include <cutils/linux/uhid.h>
#include <mutex>
#include <utility>

#ifndef u32
#define u32 uint32_t
#endif

#ifndef s32
#define s32 int32_t
#endif

#ifndef uint
#define uint uint32_t
#endif

#ifndef __s8
#define __s8 int8_t
#endif

#ifndef __s16
#define __s16 int16_t
#endif

#ifndef u8
#define u8 uint8_t
#endif

#define HID_MAX_IDS 256

/*
 * HID report descriptor global item tags
 */

#define HID_GLOBAL_ITEM_TAG_USAGE_PAGE		0
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM	1
#define HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM	2
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM	3
#define HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM	4
#define HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT	5
#define HID_GLOBAL_ITEM_TAG_UNIT		6
#define HID_GLOBAL_ITEM_TAG_REPORT_SIZE		7
#define HID_GLOBAL_ITEM_TAG_REPORT_ID		8
#define HID_GLOBAL_ITEM_TAG_REPORT_COUNT	9
#define HID_GLOBAL_ITEM_TAG_PUSH		10
#define HID_GLOBAL_ITEM_TAG_POP			11

 /*
  * HID report descriptor main item tags
  */

#define HID_MAIN_ITEM_TAG_INPUT			8
#define HID_MAIN_ITEM_TAG_OUTPUT		9
#define HID_MAIN_ITEM_TAG_FEATURE		11
#define HID_MAIN_ITEM_TAG_BEGIN_COLLECTION	10
#define HID_MAIN_ITEM_TAG_END_COLLECTION	12

#define HID_GLOBAL_STACK_SIZE 4
#define HID_COLLECTION_STACK_SIZE 4

#define HID_MAX_USAGES			12288
#define HID_DEFAULT_NUM_COLLECTIONS	16

#define HID_MIN_BUFFER_SIZE	64		/* make sure there is at least a packet size of space */
#define HID_MAX_BUFFER_SIZE	16384		/* 16kb */
#define HID_CONTROL_FIFO_SIZE	256		/* to init devices with >100 reports */
#define HID_OUTPUT_FIFO_SIZE	64

  /*
   * HID report descriptor local item tags
   */

#define HID_LOCAL_ITEM_TAG_USAGE		0
#define HID_LOCAL_ITEM_TAG_USAGE_MINIMUM	1
#define HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM	2
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_INDEX	3
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MINIMUM	4
#define HID_LOCAL_ITEM_TAG_DESIGNATOR_MAXIMUM	5
#define HID_LOCAL_ITEM_TAG_STRING_INDEX		7
#define HID_LOCAL_ITEM_TAG_STRING_MINIMUM	8
#define HID_LOCAL_ITEM_TAG_STRING_MAXIMUM	9
#define HID_LOCAL_ITEM_TAG_DELIMITER		10

#define HID_MAX_FIELDS 256

   /*
    * HID report item format
    */

#define HID_ITEM_FORMAT_SHORT	0
#define HID_ITEM_FORMAT_LONG	1

   /*
    * Special tag indicating long items
    */

#define HID_ITEM_TAG_LONG	15

#define __bitwise
#define __bitwise__ __bitwise
typedef __u16 __bitwise __le16;
typedef __u32 __bitwise __le32;

#define BIT(nr)			((1) << (nr))

#define HID_STAT_ADDED		BIT(0)
#define HID_STAT_PARSED		BIT(1)
#define HID_STAT_DUP_DETECTED	BIT(2)
#define HID_STAT_REPROBED	BIT(3)

  /*
   * HID report types --- Ouch! HID spec says 1 2 3!
   */

enum hid_report_type {
    HID_INPUT_REPORT = 0,
    HID_OUTPUT_REPORT = 1,
    HID_FEATURE_REPORT = 2,

    HID_REPORT_TYPES,
};

  /*
   * HID report descriptor collection item types
   */

#define HID_COLLECTION_PHYSICAL		0
#define HID_COLLECTION_APPLICATION	1
#define HID_COLLECTION_LOGICAL		2
#define HID_COLLECTION_NAMED_ARRAY	4

#ifdef CONFIG_ANDROID_KABI_RESERVE
#define ANDROID_KABI_RESERVE(number)			_ANDROID_KABI_RESERVE(number)
#define ANDROID_BACKPORT_RESERVE(number)		_ANDROID_BACKPORT_RESERVE(number)
#define ANDROID_BACKPORT_RESERVE_ARRAY(number, bytes)	_ANDROID_BACKPORT_RESERVE_ARRAY(number, bytes)
#else
#define ANDROID_KABI_RESERVE(number)
#define ANDROID_BACKPORT_RESERVE(number)
#define ANDROID_BACKPORT_RESERVE_ARRAY(number, bytes)
#endif

#define HID_GD_RESOLUTION_MULTIPLIER 0x00010048 

#ifndef hid_err
#define hid_err(...)
#endif

#define hid_warn hid_err
#define dbg_hid hid_err

struct list_head {
    struct list_head* next, * prev;
};

static inline void INIT_LIST_HEAD(struct list_head* list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head* new_,
    struct list_head* prev,
    struct list_head* next)
{
    next->prev = new_;
    new_->next = next;
    new_->prev = prev;
    prev->next = new_;
}

#define list_entry(ptr, type, member) \
        container_of(ptr, type, member)

#define list_first_entry(ptr, type, member) \
        list_entry((ptr)->next, type, member)

#define list_next_entry(pos, member) \
        list_entry((pos)->member.next, typeof(*(pos)), member)

#define list_for_each_entry(pos, head, member) \
	for (pos = list_first_entry(head, typeof(*pos), member); \
	     &pos->member != (head); \
	     pos = list_next_entry(pos, member))

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void list_add_tail(struct list_head* new_, struct list_head* head)
{
    __list_add(new_, head->prev, head);
}

struct hid_item {
    unsigned  format;
    __u8      size;
    __u8      type;
    __u8      tag;
    union {
        __u8   u8;
        __s8   s8;
        __u16  u16;
        __s16  s16;
        __u32  u32;
        __s32  s32;
		signed char* longdata;
    } data;
};

/*
 * This is the global environment of the parser. This information is
 * persistent for main-items. The global environment can be saved and
 * restored with PUSH/POP statements.
 */

struct hid_global {
	unsigned usage_page;
	__s32    logical_minimum;
	__s32    logical_maximum;
	__s32    physical_minimum;
	__s32    physical_maximum;
	__s32    unit_exponent;
	unsigned unit;
	unsigned report_id;
	unsigned report_size;
	unsigned report_count;
};

struct hid_local {
    unsigned usage[HID_MAX_USAGES]; /* usage array */
    u8 usage_size[HID_MAX_USAGES]; /* usage size array */
    unsigned collection_index[HID_MAX_USAGES]; /* collection index array */
    unsigned usage_index;
    unsigned usage_minimum;
    unsigned delimiter_depth;
    unsigned delimiter_branch;
};

struct hid_parser {
    struct hid_global     global;
    struct hid_global     global_stack[HID_GLOBAL_STACK_SIZE];
    unsigned int          global_stack_ptr;
    struct hid_local      local;
    unsigned int* collection_stack;
    unsigned int          collection_stack_ptr;
    unsigned int          collection_stack_size;
    struct hid_device* device;
    unsigned int          scan_flags;
    ANDROID_KABI_RESERVE(1);
};

/*
 * This is the collection stack. We climb up the stack to determine
 * application and function of each field.
 */

struct hid_collection {
    int parent_idx; /* device->collection */
    unsigned type;
    unsigned usage;
    unsigned level;
};

struct hid_report_enum {
    unsigned numbered;
    struct list_head report_list;
    struct hid_report* report_id_hash[HID_MAX_IDS];
};

/**
 * struct hid_ll_driver - low level driver callbacks
 * @start: called on probe to start the device
 * @stop: called on remove
 * @open: called by input layer on open
 * @close: called by input layer on close
 * @power: request underlying hardware to enter requested power mode
 * @parse: this method is called only once to parse the device data,
 *	   shouldn't allocate anything to not leak memory
 * @request: send report request to device (e.g. feature report)
 * @wait: wait for buffered io to complete (send/recv reports)
 * @raw_request: send raw report request to device (e.g. feature report)
 * @output_report: send output report to device
 * @idle: send idle request to device
 * @may_wakeup: return if device may act as a wakeup source during system-suspend
 * @max_buffer_size: over-ride maximum data buffer size (default: HID_MAX_BUFFER_SIZE)
 */
struct hid_ll_driver {
    int (*start)(struct hid_device* hdev);
    void (*stop)(struct hid_device* hdev);

    int (*open)(struct hid_device* hdev);
    void (*close)(struct hid_device* hdev);

    int (*power)(struct hid_device* hdev, int level);

    int (*parse)(struct hid_device* hdev);

    void (*request)(struct hid_device* hdev,
        struct hid_report* report, int reqtype);

    int (*wait)(struct hid_device* hdev);

    int (*raw_request) (struct hid_device* hdev, unsigned char reportnum,
        __u8* buf, size_t len, unsigned char rtype,
        int reqtype);

    int (*output_report) (struct hid_device* hdev, __u8* buf, size_t len);

    int (*idle)(struct hid_device* hdev, int report, int idle, int reqtype);
    bool (*may_wakeup)(struct hid_device* hdev);

    unsigned int max_buffer_size;

    ANDROID_KABI_RESERVE(1);
    ANDROID_KABI_RESERVE(2);
};

struct hid_device {
    __u8* dev_rdesc;
    unsigned dev_rsize;
    unsigned maxcollection;
    unsigned collection_size;
    struct hid_collection* collection;
    struct hid_report_enum report_enum[HID_REPORT_TYPES];
    unsigned maxapplication;					/* Number of applications */
    const struct hid_ll_driver* ll_driver;
    unsigned long status;
};

struct hid_report {
    struct list_head list;
    struct list_head hidinput_list;
    struct list_head field_entry_list;		/* ordered list of input fields */
    unsigned int id;				/* id of this report */
    enum hid_report_type type;			/* report type */
    unsigned int application;			/* application usage for this report */
    struct hid_field* field[HID_MAX_FIELDS];	/* fields of the report */
    struct hid_field_entry* field_entries;		/* allocated memory of input field_entry */
    unsigned maxfield;				/* maximum valid field index */
    unsigned size;					/* size of the report (bits) */
    struct hid_device* device;			/* associated device */

    /* tool related state */
    bool tool_active;				/* whether the current tool is active */
    unsigned int tool;				/* BTN_TOOL_* */
    ANDROID_KABI_RESERVE(1);
};

struct hid_usage {
    unsigned  hid;			/* hid usage code */
    unsigned  collection_index;	/* index into collection array */
    unsigned  usage_index;		/* index into usage array */
    __s8	  resolution_multiplier;/* Effective Resolution Multiplier
                       (HUT v1.12, 4.3.1), default: 1 */
                       /* hidinput data */
    __s8	  wheel_factor;		/* 120/resolution_multiplier */
    __u16     code;			/* input driver code */
    __u8      type;			/* input driver type */
    __s8	  hat_min;		/* hat switch fun */
    __s8	  hat_max;		/* ditto */
    __s8	  hat_dir;		/* ditto */
    __s16	  wheel_accumulated;	/* hi-res wheel */
};

struct hid_field {
    unsigned  physical;		/* physical usage for this field */
    unsigned  logical;		/* logical usage for this field */
    unsigned  application;		/* application usage for this field */
    struct hid_usage* usage;	/* usage table for this function */
    unsigned  maxusage;		/* maximum usage index */
    unsigned  flags;		/* main-item flags (i.e. volatile,array,constant) */
    unsigned  report_offset;	/* bit offset in the report */
    unsigned  report_size;		/* size of this field in the report */
    unsigned  report_count;		/* number of this field in the report */
    unsigned  report_type;		/* (input,output,feature) */
    __s32* value;		/* last known value(s) */
    __s32* new_value;		/* newly read value(s) */
    __s32* usages_priorities;	/* priority of each usage when reading the report
                     * bits 8-16 are reserved for hid-input usage
                     */
    __s32     logical_minimum;
    __s32     logical_maximum;
    __s32     physical_minimum;
    __s32     physical_maximum;
    __s32     unit_exponent;
    unsigned  unit;
    bool      ignored;		/* this field is ignored in this event */
    struct hid_report* report;	/* associated report */
    unsigned index;			/* index into report->field[] */
    /* hidinput data */
    struct hid_input* hidinput;	/* associated input structure */
    __u16 dpad;			/* dpad input code */
    unsigned int slot_idx;		/* slot index in a report */
};

bool check_mul_overflow(size_t a, size_t b, size_t* bytes)
{
    *bytes = a * b;
    return false;
}

static inline size_t array3_size(size_t a, size_t b, size_t c)
{
    size_t bytes;

    if (check_mul_overflow(a, b, &bytes))
        return SIZE_MAX;
    if (check_mul_overflow(bytes, c, &bytes))
        return SIZE_MAX;

    return bytes;
}

/*
 * Read data value from item.
 */

static u32 item_udata(struct hid_item* item)
{
	switch (item->size) {
	case 1: return item->data.u8;
	case 2: return item->data.u16;
	case 4: return item->data.u32;
	}
	return 0;
}

static s32 item_sdata(struct hid_item* item)
{
    switch (item->size) {
    case 1: return item->data.s8;
    case 2: return item->data.s16;
    case 4: return item->data.s32;
    }
    return 0;
}

static s32 snto32(__u32 value, unsigned n)
{
    if (!value || !n)
        return 0;

    if (n > 32)
        n = 32;

    switch (n) {
    case 8:  return ((__s8)value);
    case 16: return ((__s16)value);
    case 32: return ((__s32)value);
    }
    return value & (1 << (n - 1)) ? value | (~0U << n) : value;
}

s32 hid_snto32(__u32 value, unsigned n)
{
    return snto32(value, n);
}

static int hid_parser_global(struct hid_parser* parser, struct hid_item* item)
{
	__s32 raw_value;
	switch (item->tag) {
	case HID_GLOBAL_ITEM_TAG_PUSH:

		if (parser->global_stack_ptr == HID_GLOBAL_STACK_SIZE) {
			hid_err(parser->device, "global environment stack overflow\n");
			return -1;
		}

		memcpy(parser->global_stack + parser->global_stack_ptr++,
			&parser->global, sizeof(struct hid_global));
		return 0;

	case HID_GLOBAL_ITEM_TAG_POP:

		if (!parser->global_stack_ptr) {
			hid_err(parser->device, "global environment stack underflow\n");
			return -1;
		}

		memcpy(&parser->global, parser->global_stack +
			--parser->global_stack_ptr, sizeof(struct hid_global));
		return 0;

	case HID_GLOBAL_ITEM_TAG_USAGE_PAGE:
		parser->global.usage_page = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_LOGICAL_MINIMUM:
		parser->global.logical_minimum = item_sdata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_LOGICAL_MAXIMUM:
		if (parser->global.logical_minimum < 0)
			parser->global.logical_maximum = item_sdata(item);
		else
			parser->global.logical_maximum = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MINIMUM:
		parser->global.physical_minimum = item_sdata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_PHYSICAL_MAXIMUM:
		if (parser->global.physical_minimum < 0)
			parser->global.physical_maximum = item_sdata(item);
		else
			parser->global.physical_maximum = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_UNIT_EXPONENT:
		/* Many devices provide unit exponent as a two's complement
		 * nibble due to the common misunderstanding of HID
		 * specification 1.11, 6.2.2.7 Global Items. Attempt to handle
		 * both this and the standard encoding. */
		raw_value = item_sdata(item);
		if (!(raw_value & 0xfffffff0))
			parser->global.unit_exponent = hid_snto32(raw_value, 4);
		else
			parser->global.unit_exponent = raw_value;
		return 0;

	case HID_GLOBAL_ITEM_TAG_UNIT:
		parser->global.unit = item_udata(item);
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_SIZE:
		parser->global.report_size = item_udata(item);
		if (parser->global.report_size > 256) {
			hid_err(parser->device, "invalid report_size %d\n",
				parser->global.report_size);
			return -1;
		}
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_COUNT:
		parser->global.report_count = item_udata(item);
		if (parser->global.report_count > HID_MAX_USAGES) {
			hid_err(parser->device, "invalid report_count %d\n",
				parser->global.report_count);
			return -1;
		}
		return 0;

	case HID_GLOBAL_ITEM_TAG_REPORT_ID:
		parser->global.report_id = item_udata(item);
		if (parser->global.report_id == 0 ||
			parser->global.report_id >= HID_MAX_IDS) {
			hid_err(parser->device, "report_id %u is invalid\n",
				parser->global.report_id);
			return -1;
		}
		return 0;

	default:
		hid_err(parser->device, "unknown global tag 0x%x\n", item->tag);
		return -1;
	}
}

/*
 * Concatenate usage which defines 16 bits or less with the
 * currently defined usage page to form a 32 bit usage
 */

static void complete_usage(struct hid_parser* parser, unsigned int index)
{
	parser->local.usage[index] &= 0xFFFF;
	parser->local.usage[index] |=
		(parser->global.usage_page & 0xFFFF) << 16;
}

static void hid_concatenate_last_usage_page(struct hid_parser* parser)
{
	int i;
	unsigned int usage_page;
	unsigned int current_page;

	if (!parser->local.usage_index)
		return;

	usage_page = parser->global.usage_page;

	/*
	 * Concatenate usage page again only if last declared Usage Page
	 * has not been already used in previous usages concatenation
	 */
	for (i = parser->local.usage_index - 1; i >= 0; i--) {
		if (parser->local.usage_size[i] > 2)
			/* Ignore extended usages */
			continue;

		current_page = parser->local.usage[i] >> 16;
		if (current_page == usage_page)
			break;

		complete_usage(parser, i);
	}
}

/*
 * Open a collection. The type/usage is pushed on the stack.
 */

static int open_collection(struct hid_parser* parser, unsigned type)
{
	struct hid_collection* collection = nullptr;
	unsigned usage;
	int collection_index;

	usage = parser->local.usage[0];

	if (parser->collection_stack_ptr == parser->collection_stack_size) {
		unsigned int* collection_stack;
		unsigned int new_size = parser->collection_stack_size +
			HID_COLLECTION_STACK_SIZE;

		collection_stack = (unsigned int*)realloc(parser->collection_stack, new_size * sizeof(unsigned int));
		if (!collection_stack)
			return -ENOMEM;

		parser->collection_stack = collection_stack;
		parser->collection_stack_size = new_size;
	}

	if (parser->device->maxcollection == parser->device->collection_size) {
        collection = (struct hid_collection*)malloc(array3_size(sizeof(struct hid_collection),
            parser->device->collection_size,
            2));
		if (collection == NULL) {
			hid_err(parser->device, "failed to reallocate collection array\n");
			return -ENOMEM;
		}
		memcpy(collection, parser->device->collection,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		memset(collection + parser->device->collection_size, 0,
			sizeof(struct hid_collection) *
			parser->device->collection_size);
		free(parser->device->collection);
		parser->device->collection = collection;
		parser->device->collection_size *= 2;
	}

	parser->collection_stack[parser->collection_stack_ptr++] =
		parser->device->maxcollection;

	collection_index = parser->device->maxcollection++;
	collection = parser->device->collection + collection_index;
	collection->type = type;
	collection->usage = usage;
	collection->level = parser->collection_stack_ptr - 1;
	collection->parent_idx = (collection->level == 0) ? -1 :
		parser->collection_stack[collection->level - 1];

	if (type == HID_COLLECTION_APPLICATION)
		parser->device->maxapplication++;

	return 0;
}

/*
 * Close a collection.
 */

static int close_collection(struct hid_parser* parser)
{
    if (!parser->collection_stack_ptr) {
        hid_err(parser->device, "collection stack underflow\n");
        return -EINVAL;
    }
    parser->collection_stack_ptr--;
    return 0;
}

/*
 * Climb up the stack, search for the specified collection type
 * and return the usage.
 */

static unsigned hid_lookup_collection(struct hid_parser* parser, unsigned type)
{
    struct hid_collection* collection = parser->device->collection;
    int n;

    for (n = parser->collection_stack_ptr - 1; n >= 0; n--) {
        unsigned index = parser->collection_stack[n];
        if (collection[index].type == type)
            return collection[index].usage;
    }
    return 0; /* we know nothing about this usage type */
}

/*
 * Register a new report for a device.
 */

struct hid_report* hid_register_report(struct hid_device* device,
    enum hid_report_type type, unsigned int id,
    unsigned int application)
{
    struct hid_report_enum* report_enum = device->report_enum + type;
    struct hid_report* report;

    if (id >= HID_MAX_IDS)
        return NULL;
    if (report_enum->report_id_hash[id])
        return report_enum->report_id_hash[id];

    report = (struct hid_report*)malloc(sizeof(struct hid_report));
    if (!report)
        return NULL;

    memset(report, 0x00, sizeof(struct hid_report));

    if (id != 0)
        report_enum->numbered = 1;

    report->id = id;
    report->type = type;
    report->size = 0;
    report->device = device;
    report->application = application;
    report_enum->report_id_hash[id] = report;

    list_add_tail(&report->list, &report_enum->report_list);
    INIT_LIST_HEAD(&report->field_entry_list);

    return report;
}

/*
 * Register a new field for this report.
 */

static struct hid_field* hid_register_field(struct hid_report* report, unsigned usages)
{
    struct hid_field* field;

    if (report->maxfield == HID_MAX_FIELDS) {
        hid_err(report->device, "too many fields in report\n");
        return NULL;
    }

    field = (struct hid_field*)malloc((sizeof(struct hid_field) +
        usages * sizeof(struct hid_usage) +
        3 * usages * sizeof(unsigned int)));
    if (!field)
        return NULL;

    field->index = report->maxfield++;
    report->field[field->index] = field;
    field->usage = (struct hid_usage*)(field + 1);
    field->value = (s32*)(field->usage + usages);
    field->new_value = (s32*)(field->value + usages);
    field->usages_priorities = (s32*)(field->new_value + usages);
    field->report = report;

    return field;
}

/*
 * Register a new field for this report.
 */

static int hid_add_field(struct hid_parser* parser, unsigned report_type, unsigned flags)
{
    struct hid_report* report;
    struct hid_field* field;
    unsigned int max_buffer_size = HID_MAX_BUFFER_SIZE;
    unsigned int usages;
    unsigned int offset;
    unsigned int i;
    unsigned int application;

    application = hid_lookup_collection(parser, HID_COLLECTION_APPLICATION);

    report = hid_register_report(parser->device, (hid_report_type)report_type,
        parser->global.report_id, application);
    if (!report) {
        hid_err(parser->device, "hid_register_report failed\n");
        return -1;
    }

    /* Handle both signed and unsigned cases properly */
    if ((parser->global.logical_minimum < 0 &&
        parser->global.logical_maximum <
        parser->global.logical_minimum) ||
        (parser->global.logical_minimum >= 0 &&
            (__u32)parser->global.logical_maximum <
            (__u32)parser->global.logical_minimum)) {
        dbg_hid("logical range invalid 0x%x 0x%x\n",
            parser->global.logical_minimum,
            parser->global.logical_maximum);
        return -1;
    }

    offset = report->size;
    report->size += parser->global.report_size * parser->global.report_count;

    if (parser->device->ll_driver->max_buffer_size)
        max_buffer_size = parser->device->ll_driver->max_buffer_size;

    /* Total size check: Allow for possible report index byte */
    if (report->size > (max_buffer_size - 1) << 3) {
        hid_err(parser->device, "report is too long\n");
        return -1;
    }

    if (!parser->local.usage_index) /* Ignore padding fields */
        return 0;

    usages = std::max<unsigned>(parser->local.usage_index,
        parser->global.report_count);

    field = hid_register_field(report, usages);
    if (!field)
        return 0;

    field->physical = hid_lookup_collection(parser, HID_COLLECTION_PHYSICAL);
    field->logical = hid_lookup_collection(parser, HID_COLLECTION_LOGICAL);
    field->application = application;

    for (i = 0; i < usages; i++) {
        unsigned j = i;
        /* Duplicate the last usage we parsed if we have excess values */
        if (i >= parser->local.usage_index)
            j = parser->local.usage_index - 1;
        field->usage[i].hid = parser->local.usage[j];
        field->usage[i].collection_index =
            parser->local.collection_index[j];
        field->usage[i].usage_index = i;
        field->usage[i].resolution_multiplier = 1;
    }

    field->maxusage = usages;
    field->flags = flags;
    field->report_offset = offset;
    field->report_type = report_type;
    field->report_size = parser->global.report_size;
    field->report_count = parser->global.report_count;
    field->logical_minimum = parser->global.logical_minimum;
    field->logical_maximum = parser->global.logical_maximum;
    field->physical_minimum = parser->global.physical_minimum;
    field->physical_maximum = parser->global.physical_maximum;
    field->unit_exponent = parser->global.unit_exponent;
    field->unit = parser->global.unit;

    return 0;
}

static int hid_parser_main(struct hid_parser* parser, struct hid_item* item)
{
    __u32 data;
    int ret;

    hid_concatenate_last_usage_page(parser);

    data = item_udata(item);

    switch (item->tag) {
    case HID_MAIN_ITEM_TAG_BEGIN_COLLECTION:
        ret = open_collection(parser, data & 0xff);
        break;
    case HID_MAIN_ITEM_TAG_END_COLLECTION:
        ret = close_collection(parser);
        break;
    case HID_MAIN_ITEM_TAG_INPUT:
        ret = hid_add_field(parser, HID_INPUT_REPORT, data);
        break;
    case HID_MAIN_ITEM_TAG_OUTPUT:
        ret = hid_add_field(parser, HID_OUTPUT_REPORT, data);
        break;
    case HID_MAIN_ITEM_TAG_FEATURE:
        ret = hid_add_field(parser, HID_FEATURE_REPORT, data);
        break;
    default:
        hid_warn(parser->device, "unknown main item tag 0x%x\n", item->tag);
        ret = 0;
    }

    memset(&parser->local, 0, sizeof(parser->local));	/* Reset the local parser environment */

    return ret;
}

/*
 * Add a usage to the temporary parser table.
 */

static int hid_add_usage(struct hid_parser* parser, unsigned usage, u8 size)
{
    if (parser->local.usage_index >= HID_MAX_USAGES) {
        hid_err(parser->device, "usage index exceeded\n");
        return -1;
    }
    parser->local.usage[parser->local.usage_index] = usage;

    /*
     * If Usage item only includes usage id, concatenate it with
     * currently defined usage page
     */
    if (size <= 2)
        complete_usage(parser, parser->local.usage_index);

    parser->local.usage_size[parser->local.usage_index] = size;
    parser->local.collection_index[parser->local.usage_index] =
        parser->collection_stack_ptr ?
        parser->collection_stack[parser->collection_stack_ptr - 1] : 0;
    parser->local.usage_index++;
    return 0;
}

/*
 * Process a local item.
 */

static int hid_parser_local(struct hid_parser* parser, struct hid_item* item)
{
    __u32 data;
    unsigned n;
    __u32 count;

    data = item_udata(item);

    switch (item->tag) {
    case HID_LOCAL_ITEM_TAG_DELIMITER:

        if (data) {
            /*
             * We treat items before the first delimiter
             * as global to all usage sets (branch 0).
             * In the moment we process only these global
             * items and the first delimiter set.
             */
            if (parser->local.delimiter_depth != 0) {
                hid_err(parser->device, "nested delimiters\n");
                return -1;
            }
            parser->local.delimiter_depth++;
            parser->local.delimiter_branch++;
        }
        else {
            if (parser->local.delimiter_depth < 1) {
                hid_err(parser->device, "bogus close delimiter\n");
                return -1;
            }
            parser->local.delimiter_depth--;
        }
        return 0;

    case HID_LOCAL_ITEM_TAG_USAGE:

        if (parser->local.delimiter_branch > 1) {
            dbg_hid("alternative usage ignored\n");
            return 0;
        }

        return hid_add_usage(parser, data, item->size);

    case HID_LOCAL_ITEM_TAG_USAGE_MINIMUM:

        if (parser->local.delimiter_branch > 1) {
            dbg_hid("alternative usage ignored\n");
            return 0;
        }

        parser->local.usage_minimum = data;
        return 0;

    case HID_LOCAL_ITEM_TAG_USAGE_MAXIMUM:

        if (parser->local.delimiter_branch > 1) {
            dbg_hid("alternative usage ignored\n");
            return 0;
        }

        count = data - parser->local.usage_minimum;
        if (count + parser->local.usage_index >= HID_MAX_USAGES) {
            /*
             * We do not warn if the name is not set, we are
             * actually pre-scanning the device.
             */
#ifndef _MSC_VER
            if (dev_name(&parser->device->dev))
                hid_warn(parser->device,
                    "ignoring exceeding usage max\n");
#endif
            data = HID_MAX_USAGES - parser->local.usage_index +
                parser->local.usage_minimum - 1;
            if (data <= 0) {
                hid_err(parser->device,
                    "no more usage index available\n");
                return -1;
            }
        }

        for (n = parser->local.usage_minimum; n <= data; n++)
            if (hid_add_usage(parser, n, item->size)) {
                dbg_hid("hid_add_usage failed\n");
                return -1;
            }
        return 0;

    default:

        dbg_hid("unknown local item tag 0x%x\n", item->tag);
        return 0;
    }
    return 0;
}

/*
 * Process a reserved item.
 */

static int hid_parser_reserved(struct hid_parser* parser, struct hid_item* item)
{
    dbg_hid("reserved item type, tag 0x%x\n", item->tag);
    return 0;
}

/*
 * Fetch a report description item from the data stream. We support long
 * items, though they are not used yet.
 */

static u8* fetch_item(__u8* start, __u8* end, struct hid_item* item)
{
    u8 b;

    if ((end - start) <= 0)
        return NULL;

    b = *start++;

    item->type = (b >> 2) & 3;
    item->tag = (b >> 4) & 15;

    if (item->tag == HID_ITEM_TAG_LONG) {

        item->format = HID_ITEM_FORMAT_LONG;

        if ((end - start) < 2)
            return NULL;

        item->size = *start++;
        item->tag = *start++;

        if ((end - start) < item->size)
            return NULL;

        item->data.longdata = (signed char*)start;
        start += item->size;
        return start;
    }

    item->format = HID_ITEM_FORMAT_SHORT;
    item->size = b & 3;

    switch (item->size) {
    case 0:
        return start;

    case 1:
        if ((end - start) < 1)
            return NULL;
        item->data.u8 = *start++;
        return start;

    case 2:
        if ((end - start) < 2)
            return NULL;
        // todo remove. j.w item->data.u16 = get_unaligned_le16(start);
        start = (__u8*)((__le16*)start + 1);
        return start;

    case 3:
        item->size++;
        if ((end - start) < 4)
            return NULL;
        // todo remove. j.w item->data.u32 = get_unaligned_le32(start);
        start = (__u8*)((__le32*)start + 1);
        return start;
    }

    return NULL;
}

static void hid_apply_multiplier_to_field(struct hid_device* hid,
    struct hid_field* field,
    struct hid_collection* multiplier_collection,
    int effective_multiplier)
{
    struct hid_collection* collection;
    struct hid_usage* usage;
    int i;

    /*
     * If multiplier_collection is NULL, the multiplier applies
     * to all fields in the report.
     * Otherwise, it is the Logical Collection the multiplier applies to
     * but our field may be in a subcollection of that collection.
     */
    for (i = 0; i < field->maxusage; i++) {
        usage = &field->usage[i];

        collection = &hid->collection[usage->collection_index];
        while (collection->parent_idx != -1 &&
            collection != multiplier_collection)
            collection = &hid->collection[collection->parent_idx];

        if (collection->parent_idx != -1 ||
            multiplier_collection == NULL)
            usage->resolution_multiplier = effective_multiplier;

    }
}

static int hid_calculate_multiplier(struct hid_device* hid,
    struct hid_field* multiplier)
{
    int m;
    __s32 v = *multiplier->value;
    __s32 lmin = multiplier->logical_minimum;
    __s32 lmax = multiplier->logical_maximum;
    __s32 pmin = multiplier->physical_minimum;
    __s32 pmax = multiplier->physical_maximum;

    /*
     * "Because OS implementations will generally divide the control's
     * reported count by the Effective Resolution Multiplier, designers
     * should take care not to establish a potential Effective
     * Resolution Multiplier of zero."
     * HID Usage Table, v1.12, Section 4.3.1, p31
     */
    if (lmax - lmin == 0)
        return 1;
    /*
     * Handling the unit exponent is left as an exercise to whoever
     * finds a device where that exponent is not 0.
     */
    m = ((v - lmin) / (lmax - lmin) * (pmax - pmin) + pmin);
    if ((multiplier->unit_exponent != 0)) {
        hid_warn(hid,
            "unsupported Resolution Multiplier unit exponent %d\n",
            multiplier->unit_exponent);
    }

    /* There are no devices with an effective multiplier > 255 */
    if ((m == 0 || m > 255 || m < -255)) {
        hid_warn(hid, "unsupported Resolution Multiplier %d\n", m);
        m = 1;
    }

    return m;
}


static void hid_apply_multiplier(struct hid_device* hid,
    struct hid_field* multiplier)
{
    struct hid_report_enum* rep_enum;
    struct hid_report* rep;
    struct hid_field* field;
    struct hid_collection* multiplier_collection;
    int effective_multiplier;
    int i;

    /*
     * "The Resolution Multiplier control must be contained in the same
     * Logical Collection as the control(s) to which it is to be applied.
     * If no Resolution Multiplier is defined, then the Resolution
     * Multiplier defaults to 1.  If more than one control exists in a
     * Logical Collection, the Resolution Multiplier is associated with
     * all controls in the collection. If no Logical Collection is
     * defined, the Resolution Multiplier is associated with all
     * controls in the report."
     * HID Usage Table, v1.12, Section 4.3.1, p30
     *
     * Thus, search from the current collection upwards until we find a
     * logical collection. Then search all fields for that same parent
     * collection. Those are the fields the multiplier applies to.
     *
     * If we have more than one multiplier, it will overwrite the
     * applicable fields later.
     */
    multiplier_collection = &hid->collection[multiplier->usage->collection_index];
    while (multiplier_collection->parent_idx != -1 &&
        multiplier_collection->type != HID_COLLECTION_LOGICAL)
        multiplier_collection = &hid->collection[multiplier_collection->parent_idx];
    if (multiplier_collection->type != HID_COLLECTION_LOGICAL)
        multiplier_collection = NULL;

    effective_multiplier = hid_calculate_multiplier(hid, multiplier);

    rep_enum = &hid->report_enum[HID_INPUT_REPORT];
#ifdef _MSC_VER
    for (auto it = &(rep_enum->report_list); it != &(rep_enum->report_list); it = it->next) {
        rep = reinterpret_cast<struct hid_report*>(it);
#else
    list_for_each_entry(rep, &rep_enum->report_list, list) {
#endif
        for (i = 0; i < rep->maxfield; i++) {
            field = rep->field[i];
            hid_apply_multiplier_to_field(hid, field,
                multiplier_collection,
                effective_multiplier);
        }
    }
}

/*
 * hid_setup_resolution_multiplier - set up all resolution multipliers
 *
 * @device: hid device
 *
 * Search for all Resolution Multiplier Feature Reports and apply their
 * value to all matching Input items. This only updates the internal struct
 * fields.
 *
 * The Resolution Multiplier is applied by the hardware. If the multiplier
 * is anything other than 1, the hardware will send pre-multiplied events
 * so that the same physical interaction generates an accumulated
 *	accumulated_value = value * * multiplier
 * This may be achieved by sending
 * - "value * multiplier" for each event, or
 * - "value" but "multiplier" times as frequently, or
 * - a combination of the above
 * The only guarantee is that the same physical interaction always generates
 * an accumulated 'value * multiplier'.
 *
 * This function must be called before any event processing and after
 * any SetRequest to the Resolution Multiplier.
 */
void hid_setup_resolution_multiplier(struct hid_device* hid)
{
    struct hid_report_enum* rep_enum;
    struct hid_report* rep;
    struct hid_usage* usage;
    int i, j;

    rep_enum = &hid->report_enum[HID_FEATURE_REPORT];
#ifdef _MSC_VER
    for(auto it = &(rep_enum->report_list); it != &(rep_enum->report_list); it = it->next) {
        rep = reinterpret_cast<struct hid_report*>(it);
#else
    list_for_each_entry(rep, &rep_enum->report_list, list) {
#endif
        for (i = 0; i < rep->maxfield; i++) {
            /* Ignore if report count is out of bounds. */
            if (rep->field[i]->report_count < 1)
                continue;

            for (j = 0; j < rep->field[i]->maxusage; j++) {
                usage = &rep->field[i]->usage[j];
                if (usage->hid == HID_GD_RESOLUTION_MULTIPLIER)
                    hid_apply_multiplier(hid,
                        rep->field[i]);
            }
        }
    }
}

int hid_open_report(struct hid_device* device)
{
    struct hid_parser* parser = nullptr;
    struct hid_item item;
    __u8* next;
    unsigned int size;
    __u8* start;
    __u8* buf;
    __u8* end;

    size = device->dev_rsize;
    start = device->dev_rdesc;
    end = start + size;
    parser = (struct hid_parser*)malloc(sizeof(struct hid_parser));
    if(parser!= nullptr)
    {
        memset(parser, 0x00, sizeof(struct hid_parser));
        parser->device = device;
    }

    device->collection = (struct hid_collection*)malloc(HID_DEFAULT_NUM_COLLECTIONS*
        sizeof(struct hid_collection));
    memset(device->collection, 0x00, HID_DEFAULT_NUM_COLLECTIONS *
        sizeof(struct hid_collection));
    device->collection_size = HID_DEFAULT_NUM_COLLECTIONS;
    for (int i = 0; i < HID_DEFAULT_NUM_COLLECTIONS; i++)
    {
        if (device->collection)
        {
            device->collection[i].parent_idx = -1;
        }
    }

    static int (*dispatch_type[])(struct hid_parser*,
        struct hid_item*) = {
    hid_parser_main,
    hid_parser_global,
    hid_parser_local,
    hid_parser_reserved
    };

    while ((next = fetch_item(start, end, &item)) != NULL) {
        start = next;

        if (item.format != HID_ITEM_FORMAT_SHORT) {
            hid_err(device, "unexpected long global item\n");
            goto err;
        }

        if (dispatch_type[item.type](parser, &item)) {
            hid_err(device, "item %u %u %u %u parsing failed\n",
                item.format, (unsigned)item.size,
                (unsigned)item.type, (unsigned)item.tag);
            goto err;
        }

        if (start == end) {
            if (parser->collection_stack_ptr) {
                hid_err(device, "unbalanced collection at end of report description\n");
                goto err;
            }
            if (parser->local.delimiter_depth) {
                hid_err(device, "unbalanced delimiter at end of report description\n");
                goto err;
            }

            /*
                * fetch initial values in case the device's
                * default multiplier isn't the recommended 1
                */
            hid_setup_resolution_multiplier(device);

            free(parser->collection_stack);
            free(parser);
            device->status |= HID_STAT_PARSED;

            return 0;
        }
    }

err:

    return 0;
}

void parse_hid_descriptor(void* desc, int32_t size)
{
    hid_device device;
    memset(&device, 0x00, sizeof(device));
    device.dev_rdesc = (__u8*)desc;
    device.dev_rsize = size;
    device.maxapplication = 0;
    for (int i = 0; i < HID_REPORT_TYPES; ++i)
    {
        device.report_enum[i].numbered;
        INIT_LIST_HEAD(&(device.report_enum[i].report_list));
    }
    auto ll_driver = (struct hid_ll_driver*)malloc(sizeof(struct hid_ll_driver));
    if (ll_driver)
    {
        memset(ll_driver, 0x00, sizeof(struct hid_ll_driver));
        ll_driver->max_buffer_size = HID_MAX_BUFFER_SIZE;
    }
    device.ll_driver = ll_driver;

    hid_open_report(&device);
}

