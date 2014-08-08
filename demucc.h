#ifndef demucc_h
#define demucc_h

#include <inttypes.h>
#include <stdio.h>

#define LogL(format, ...) \
	fprintf(stderr, ("%s:%s(%d): " format "\n"), __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define TPQN 24
#define STACK_SIZE 32 // muslib ¤Ï 24
#define DRUMNAME_MAX 16

struct pmd {
	/* global context */
	uint8_t* buffer;
	long buffer_size;
	uint8_t* p;
	uint8_t** parts;
	uint8_t* drums;
	int num_parts;
	char* title;
	char* title2;

	/* current part context */
	int stack[STACK_SIZE];
	int sp;
	int len;
	int drum_track;
	int legato;
    int oct;
    int porpd;
    int porlen;
};

int read_notes(struct pmd* pmd, uint8_t** pp);
int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp);
int extract_drums(struct pmd* pmd);

/* util.c */
uint16_t read_u16(uint8_t** pp);
uint8_t read_u8(uint8_t** pp);
int tick2beat(int tick, char* notes);
void get_note(struct pmd* pmd, int note, int tick);
int get_drumname(int note, char* buffer, size_t size);
void reset_part_ctx(struct pmd* pmd);

/* commands.c */
int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp);

#endif // !demucc_h
