#ifndef	TREE_IO_H
#define	TREE_IO_H

#include <glib.h>
#include "tree.h"

Tree * tree_io_load(const gchar *fname);
void tree_io_save(Tree *tree, const gchar *fname);
void tree_io_save_io(Tree *tree, GIOChannel *io);

#endif /*TREE_IO_H*/
