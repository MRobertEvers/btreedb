#ifndef PAGER_H_
#define PAGER_H_

enum pager_e
{
	PAGER_OK,
	PAGER_UNK_ERR,
	PAGER_OPEN_ERR,
	PAGER_SEEK_ERR,
	PAGER_READ_ERR,
	PAGER_WRITE_ERR,
};

struct Pager
{
	char pager_name_str[32];
	int page_size;
	void* file;
};

struct Page
{
	int page_id;

	void* page_buffer;
};

enum pager_e pager_page_alloc(struct Pager*, struct Page**);
enum pager_e pager_page_dealloc(struct Page*);
enum pager_e pager_page_init(struct Pager*, struct Page*, int);
enum pager_e pager_page_deinit(struct Pager*, struct Page*);

enum pager_e pager_alloc(struct Pager**);
enum pager_e pager_dealloc(struct Pager**);
enum pager_e pager_init(struct Pager*, int);
enum pager_e pager_open(struct Pager*, char*);
enum pager_e pager_close(struct Pager*);
enum pager_e pager_read_page(struct Pager*, struct Page*);
enum pager_e pager_write_page(struct Pager*, struct Page*);

#endif