/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifndef demucc_h
#define demucc_h

#include <inttypes.h>
#include <stdio.h>

#define LogL(format, ...) \
	fprintf(stderr, ("%s:%s(%d): " format "\n"), __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define TPQN 24
#define TIMEBASE (TPQN * 4)
#define STACK_SIZE 32 // muslib は 24
#define DRUMNAME_MAX 16
#define MMLBUF_SIZE (8 * 1024)
#define MML_COLUMNS 78

struct mmlbuf {
    size_t size;
    char* buf;
    char* mml;
};

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
    int part;
	int stack[STACK_SIZE];
	int sp;
	int len;
	int drum_track;
	int legato;
    int oct;
    int porsw;
    int porpd;
    int porlen;
    int return_addr;

    struct mmlbuf* mmlbuf;
    int column;
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
int mml_printf(struct pmd* pmd, char* format, ...)
    __attribute__((format(printf, 2, 3)));
int mml_vprintf(struct pmd* pmd, char* format, va_list vp)
    __attribute__((format(printf, 2, 0)));

/* commands.c */
int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp);

/* mmlbuf.c */
struct mmlbuf* mmlbuf_new();
void mmlbuf_free(struct mmlbuf* mmlbuf);
void mmlbuf_reset(struct mmlbuf* mmlbuf);
char* mmlbuf_buf(struct mmlbuf* mmlbuf);
size_t mmlbuf_size(struct mmlbuf* mmlbuf);
size_t mmlbuf_pos(struct mmlbuf* mmlbuf);
size_t mmlbuf_left(struct mmlbuf* mmlbuf);
int mmlbuf_append(struct mmlbuf* mmlbuf, char* format, ...) 
    __attribute__((format(printf, 2, 3)));
int mmlbuf_appendv(struct mmlbuf* mmlbuf, char* format, va_list ap)
    __attribute__((format(printf, 2, 0)));



#endif // !demucc_h
