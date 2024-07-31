## Format on Save
- The .clang-format file found in this directory will be used
- It uses the Google style with longer line limits and some macro readability improvements. 
- This file is not set in stone, please feel free to suggest updates.

## Parenthesis
- Use them everywhere. Assume the other person reading the code doesn't know operator precedence. 

## Keywords to frequent
- static
- const
- volatile (where appropriate)
- double (avoid 32-bit float issues)

## Typedefs
- Generally avoid typedefs
- Do not use for structs or enums
- Allowed for fixed width integer types (more below)

## Interger representation
- Use fixed width integer type almost always. Ex `uint32_t foo = 42;`
- Use fixed width macros for string formatting. Ex `printf("Number: %" PRIu32 "\n", number);`

## String formatting
- All string formatting will use `snprintf()` unless there is a good reason not to.
- Exception: using printf is fine for simple string formatting.

## Functions
- Generally keep functions less than 50 lines
- Exception: Long case statements, where each case is only a couple lines of code
- Keep local variable to less then 5~10
- Keep indentation level low. Invert your logic and split your loops if needed. 

## Function Prototypes
- Keep variable names and types

## Early exits and jumps
- Try to only have one return statement per function. There are exceptions.
- When using early exits or jumps clearly label where they happen in code
- Give jumps descriptive names
- Note: Jumps are usually for cleaning up memory allocation for early exits

## Braces
- Always use them, even for one line if statements

## Commenting
- Describe What code does, sometimes Why, and not How it works.
- Describe all public interfaces in header files
- Do not comment out large sections of code, if needed for testing wrap in if statement and add config variable
- Todo: Should this project use doxygen type comments?

## Macros
- Avoid `#define constant`, and use `static type name = val` instead.
- Use enums when defining related constants
- Macros with multiple statements should use a do while loop to protect them
- Macros should not effect control flow (very bad)
- Functions are preferred to macros if equivelency can be made

## Naming 
- Use descriptive naming for any global variables or interfaces
- Use descriptive naming when in doubt
- Exceptions are loop counters and internal variables to short functions

### Modules
- .c and .h files should be snake case (all_lower_case_with_underscores)

### Functions
- Functions should snake case (all_lower_case_with_underscores)
- Functions returning a value with units should be postfixed with the unit they are returning i.e. `uint32_t get_time_ms()`

### Types
- For enums, structs, typedefs(used judicially), etc, use upper camel case (UpperCamelCase)

### Macros
- If you absolutely have to use a macro, use UPPER_CASE naming convention

### Static Const for #define replacement
- Same as macros, UPPER_CASE

### Variables
- All variable names shall use snake_case unless otherwise called out below
- enum variables shall use the UPPER_CASE naming convention, make sure you don't create collisions with macros
- Variables should always end with their units if they are a measure of something (`float magnetic_flux_uT = 0;` - micro Tesla)
- Capital letters are allowed for units. For very common units like seconds, keep them lower case.

## Allocating memory
- The FreeRTOS memory allocator is preferred.

## Casting
- Cast away from void* as soon as possible
- If casting, ask yourself if the original type should change 

## Zeroing memory
- use {0} when defining, otherwise use memset

## Early optimization
- Don't inline functions unless you have data to support it's use
- If the task stack watermark is less than half, make it larger
  - Exception: We start to approach 70% max heap usage watermark
- Only use floats, instead of doubles, if optimization calls for it
- Only use fixed point math if optimization calls for it

## Conditional Compilation
- Try to avoid in general
- Leave a comment at the end of #endif 

## Resetting the device and panicking
- Try to avoid this and fail gracefully
- Exceptions, if we can not connect to wi-fi we are dead in the water.
  - Todo: consider auto return to dock if we loose wi-fi after multiple reboots and still have the compass

## Inspriation for this Guide
- Personal Experience - Style guides are highly opinionated
- Linux kernel coding style https://www.kernel.org/doc/html/latest/process/coding-style.html
- Barr's Embedded C Coding Standard https://barrgroup.com/embedded-systems/books/embedded-c-coding-standard
- Ben Klemens, 21st Century C
- Google C++ Style Guide https://google.github.io/styleguide/cppguide.html
