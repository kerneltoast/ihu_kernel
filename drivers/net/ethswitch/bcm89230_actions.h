#ifndef BCM89230_ACTIONS_H
#define BCM89230_ACTIONS_H

#define MASK_FULL 0xFFFFFFFFFFFFFFFF

enum ACTION {
	SET_VALUE,      // write new value to register
	SET_VERIFY,     // write new value to register and verify the write by reading back the value
	CHECK_WARN,     // read the value of the register and warn if it doesn't match the expected value
	CHECK_FAIL,     // read the value of the register and abort if it doesn't match the expected value
	PRINT_REG,      // print value of register
	GET_VALUE,      // return value of register
	CHECK_VALUE,    // check if value is same as expected silently
};

typedef struct ACTION_ITEM {
	u32 register_address;      // address of register to operate on
	unsigned int register_size;     // size of register in bits 8, 16, 32 or 64
	enum ACTION action;             // action to perform (one value from the ACTION enum)
	u64 mask;                  // bit mask to apply
	u64 value;                 // expected / new value
	char *comment;                  // any comment or NULL
} ACTION_ITEM;

#endif
