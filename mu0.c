#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define LINE_SIZE 90
#define MAX_LABEL_SIZE 90
#define IO_ADDRESS 0xfff

#define LABEL_C ':'
#define NUM_LITERAL_C '#'
#define CHAR_LITERAL_C '$'
#define COMMENT_C ';'

#define USAGE "Usage:\n\n"\
    "1. mu0 assemble <assembly file> <machine code file> [-v]\n"\
    "2. mu0 emulate <machine code file> [-v] [-l n]\n\n"\
    "    -v  : verbose\n"\
    "    -l n: limit on the number of clock cycles to emulate\n"\
    "\n"\
    "The assembler chooses what to do with each line based on the first character(s)\n"\
    "of the line. If the first character is:\n"\
    "    ';' or whitespace the line is ignored.\n"\
    "    ':' the next word is assumed to be a label.\n"\
    "    '#' the next number is stored at the next memory location.\n"\
    "    '$' the next character is stored as its ASCII representation.\n"\
    "If the line starts with one of the three letter commands\n"\
    "    LDA, STO, ADD, SUB, JMP, JGE, JNE\n"\
    "The opcode is stored and the next token is assumed to be the memory address.\n"\
    "If the memory address starts with a ':' it is assumed to be a label.\n"\
    "If the line starts with STP, 0 is stored at the next memory location.\n"\
    "\n"\
    "The emulator expects a sequence of 4 digit hex numbers, one per line.\n"\
    "Memory location 0xfff is interpreted as memory-mapped IO. A LDA from 0xfff \n"\
    "reads from stdin and a STO to 0xfff prints to stdout.\n"\
    "\n"\
    "Warnings: Lines must not exceed 90 characters\n"\
    "    The code is not very robust. If the files don't match the requirements, \n"\
    "    behaviour is undefined.\n"
    

enum opcode_t {
	LDA = 0,
	STO = 1,
	ADD = 2,
	SUB = 3,
	JMP = 4,
	JGE = 5,
	JNE = 6,
	STP = 7
};

static char *opcode_str[8] = {
    "LDA", "STO", "ADD", "SUB", "JMP", "JGE", "JNE", "STP"
};

enum state_t {
    FETCH,
    EXECUTE
};

typedef struct {
    unsigned int size;
    unsigned int *data;
} memory_t;

/* Labels stored in a linked list */
typedef struct label_table_t {
    char *label;
    int address;
    struct label_table_t *next;
} label_table_t;

/* ------------------------------------------- */
/* ---------------- ASSEMBLER ---------------- */
/* ------------------------------------------- */

label_table_t *add_label(label_table_t *table, char *label, int address)
{
    label_table_t *new_table = malloc(sizeof(label_table_t));
    new_table->label = strdup(label);
    new_table->address = address;
    new_table->next = table;
    return new_table;
}

/* returns -1 if not found */
int get_address(label_table_t *table, char *label)
{
    if (table == NULL)
    {
        return -1;
    }
    else if (!strcmp(label, table->label))
    {
        return table->address;
    }
    else
    {
        return get_address(table->next, label);
    }
}

void free_table(label_table_t *table)
{
    if (table != NULL)
    {
        free_table(table->next);
        free(table->label);
        free(table);
    }
}

/* First pass of the file to resolve all the labels */
label_table_t *generate_label_table(FILE *fin, int verbose)
{
    label_table_t *table = NULL;
    char line[LINE_SIZE];
    char label[MAX_LABEL_SIZE];
    int addr = 0;
    
    while (fgets(line, LINE_SIZE, fin) != NULL)
    {
        if (*line == LABEL_C)
        {
            sscanf(line + 1, "%s", label);
            if (verbose)
            {
                printf("Found label definition \"%s\" at address %x\n", label, addr);
            }
            table = add_label(table, label, addr);
        }
        else if (!isspace(*line) && (*line) != COMMENT_C)
        {
            addr++;
        }
    }
    rewind(fin);
    return table;
}

/* Returns if the line was successfully parsed */
int process_opcode(char *line, label_table_t *table, FILE *fout, int verbose)
{
    int i;
    char addr_s[MAX_LABEL_SIZE];
    int addr;
    for (i = 0; i < 8; i++)
    {
        if (!strncmp(line, opcode_str[i], 3)) {
            /* Found opcode. Skip over opcode and get address. 
             * If the opcode is STP there shouldn't be a memory address
             * and this gives the empty string which becomes zero.
             * It's just a fluke that this works but less cases is better, right? */
            sscanf(line + 3, "%s", addr_s);
            if (*addr_s == LABEL_C)
            {
                addr = get_address(table, addr_s + 1);
                if (addr < 0)
                {
                    fprintf(stderr, "Unknown label \"%s\"\n", addr_s + 1);
                    exit(1);
                }
            }
            else
            {
                addr = strtol(addr_s, NULL, 0);
            }
            fprintf(fout, "%01x%03x\n", i, addr);
            return 1;
        }
    }
    return 0;
}

void assemble(FILE *fin, FILE *fout, int verbose) 
{
    label_table_t *table;
    char line[LINE_SIZE];
    int line_ok;

    table = generate_label_table(fin, verbose);
    /* Iterate through all the lines. This is full of pointer magic
     * which is why the code is so fragile.
     */
    while (fgets(line, LINE_SIZE, fin) != NULL)
    {
        line_ok = 0;
        if (*line == LABEL_C || *line == COMMENT_C || isspace(*line))
        {
            /* ignore line */
            line_ok = 1;
        }
        else if (*line == NUM_LITERAL_C)
        {
            fprintf(fout, "%04lx\n", strtol(line+1, NULL, 0));
            line_ok = 1;
        }
        else if (*line == CHAR_LITERAL_C)
        {
            fprintf(fout, "%04x\n", (int) line[1]);
            line_ok = 1;
        }
        else
        {
            line_ok = process_opcode(line, table, fout, verbose);
        }
        if (!line_ok)
        {
            fprintf(stderr, "Warning: Ignoring bad line: %s", line);
        }
    }
    free_table(table);
	return;
}

/* ------------------------------------------ */
/* ---------------- EMULATOR ---------------- */
/* ------------------------------------------ */

int mem_size(FILE *fin)
{
    int size = 0;
    int operation;
    while (fscanf(fin, "%x", &operation) == 1)
    {
        size++;
    }
    rewind(fin);
    return size;
}

/* one instruction per line, nothing else in file, addresses as hex */
memory_t *read_machine_code(FILE *fin, int verbose)
{
    memory_t *mem;
    int i;
    mem = malloc(sizeof(memory_t));
    if (mem == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }
    mem->size = mem_size(fin);
    mem->data = malloc(mem->size * sizeof(int));
    if (mem->data == NULL)
    {
        fprintf(stderr, "Memory allocation error\n");
        exit(1);
    }
    for (i = 0; i < mem->size; i++)
    {
        fscanf(fin, "%x", mem->data + i);
    }
    if (verbose)
    {
        fprintf(stderr, "Read in %d lines\n", mem->size);
    }
    return mem;
}

void free_mem(memory_t *mem)
{
    if (mem != NULL)
    {
        free(mem->data);
        free(mem);
    }
}

enum opcode_t get_opcode(int x)
{
    return x >> 12;
}

int get_operand(int x)
{
    return x & 0xfff;
}

int get(memory_t *mem, int address)
{
    int x;
    char c;
    if (address == IO_ADDRESS)
    {
        scanf("%c", &c);
        x = (int) c;
    }
    else if (address > mem->size)
    {
        fprintf(stderr, "Memory address 0x%x is out of range\n", address);
        // exit code for SIGSEGV
        exit(139);
    }
    else
    {
        x = mem->data[address];
    }
    return x;
}

void set(memory_t *mem, int address, int value)
{
    if (address == IO_ADDRESS)
    {
        printf("%c", value);
    }
    else if (address > mem->size)
    {
        fprintf(stderr, "Memory address 0x%x is out of range\n", address);
        exit(139);
    }
    else
    {
        mem->data[address] = value;
    }
}

void emulate(memory_t *mem, int verbose, int limit)
{
	int PC = 0;
    int ACC = 0;
    int IR = 0;
    enum state_t state = FETCH;
    int done = 0;
    int steps = 0;
    while (!done && (limit <= 0 || steps < limit))
    {
        steps++;
        if (verbose)
        {
            fprintf(stderr, "%3d: state = %7s, PC = %04x, ACC = %04x, IR = %04x\n", 
                steps, state == FETCH ? "FETCH" : "EXECUTE", PC, ACC, IR);
        }
        if (state == FETCH)
        {
            IR = get(mem, PC++);
            state = EXECUTE;
        }
        else
        {
            switch (get_opcode(IR))
            {
                case LDA:
                    ACC = get(mem, get_operand(IR));
                    state = FETCH;
                    break;
                case STO:
                    set(mem, get_operand(IR), ACC);
                    state = FETCH;
                    break;
                case ADD:
                    ACC += get(mem, get_operand(IR));
                    state = FETCH;
                    break;
                case SUB:
                    ACC -= get(mem, get_operand(IR));
                    state = FETCH;
                    break;
                case JMP:
                    PC = get_operand(IR);
                    IR = get(mem, PC++);
                    break;
                case JGE:
                    if (ACC >= 0)
                    {
                        PC = get_operand(IR);
                        IR = get(mem, PC++);
                    }
                    else
                    {
                        state = FETCH;
                    }
                    break;
                case JNE:
                    if (ACC != 0)
                    {
                        PC = get_operand(IR);
                        IR = get(mem, PC++);
                    }
                    else
                    {
                        state = FETCH;
                    }
                    break;
                case STP:
                    done = 1;
                    break;
            }
        }
    }
    if (!done)
    {
        fprintf(stderr, "Step limit exceeded\n");
    }
}

int is_verbose(int argc, char **argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-v"))
        {
            return 1;
        }
    }
    return 0;
}

/* Returns zero if not specified */
int step_limit(int argc, char **argv)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        if (!strcmp(argv[i], "-l"))
        {
            if (argc > i + 1)
            {
                return (int) strtol(argv[i+1], NULL, 0);
            }
            else
            {
                fprintf(stderr, "Must give step limit with -l\n");
                exit(1);
            }
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    memory_t *mem;
    FILE *fin;
    FILE *fout;
    int verbose;
    int limit;
	if (argc < 3)
    {
        fprintf(stderr, "%s", USAGE);
        exit(1);
    }
    verbose = is_verbose(argc, argv);
    limit = step_limit(argc, argv);
    if (!strcmp(argv[1], "emulate"))
    {
        fin = fopen(argv[2], "r");
        mem = read_machine_code(fin, verbose);
        emulate(mem, verbose, limit);
        free_mem(mem);
        fclose(fin);
    }
    else if (!strcmp(argv[1], "assemble"))
    {
        if (argc < 4)
        {
            fprintf(stderr, "Not enough arguments to assemble\n");
            exit(1);
        }
        fin = fopen(argv[2], "r");
        fout = fopen(argv[3], "w");
        assemble(fin, fout, verbose);
        fclose(fout);
        fclose(fin);
    }
    else
    {
        printf("Unknown command %s\n", argv[1]);
    }
	return 0;
}
