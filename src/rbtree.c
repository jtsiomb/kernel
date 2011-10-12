#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rbtree.h"
#include "panic.h"

#define INT2PTR(x)	((void*)(x))
#define PTR2INT(x)	((int)(x))

static int cmpaddr(void *ap, void *bp);
static int cmpint(void *ap, void *bp);

static int count_nodes(struct rbnode *node);
static void del_tree(struct rbnode *node, void (*delfunc)(struct rbnode*, void*), void *cls);
static struct rbnode *insert(struct rbtree *rb, struct rbnode *tree, void *key, void *data);
static struct rbnode *delete(struct rbtree *rb, struct rbnode *tree, void *key);
static void traverse(struct rbnode *node, void (*func)(struct rbnode*, void*), void *cls);

struct rbtree *rb_create(rb_cmp_func_t cmp_func)
{
	struct rbtree *rb;

	if(!(rb = malloc(sizeof *rb))) {
		return 0;
	}
	if(rb_init(rb, cmp_func) == -1) {
		free(rb);
		return 0;
	}
	return rb;
}

void rb_free(struct rbtree *rb)
{
	rb_destroy(rb);
	free(rb);
}


int rb_init(struct rbtree *rb, rb_cmp_func_t cmp_func)
{
	memset(rb, 0, sizeof *rb);

	if(cmp_func == RB_KEY_INT) {
		rb->cmp = cmpint;
	} else if(cmp_func == RB_KEY_STRING) {
		rb->cmp = (rb_cmp_func_t)strcmp;
	} else {
		rb->cmp = cmpaddr;
	}

	rb->alloc = malloc;
	rb->free = free;
	return 0;
}

void rb_destroy(struct rbtree *rb)
{
	del_tree(rb->root, rb->del, rb->del_cls);
}

void rb_clear(struct rbtree *rb)
{
	del_tree(rb->root, rb->del, rb->del_cls);
	rb->root = 0;
}

int rb_copy(struct rbtree *dest, struct rbtree *src)
{
	struct rbnode *node;

	rb_clear(dest);

	rb_begin(src);
	while((node = rb_next(src))) {
		if(rb_insert(dest, node->key, node->data) == -1) {
			return -1;
		}
	}
	return 0;
}

void rb_set_allocator(struct rbtree *rb, rb_alloc_func_t alloc, rb_free_func_t free)
{
	rb->alloc = alloc;
	rb->free = free;
}


void rb_set_compare_func(struct rbtree *rb, rb_cmp_func_t func)
{
	rb->cmp = func;
}

void rb_set_delete_func(struct rbtree *rb, rb_del_func_t func, void *cls)
{
	rb->del = func;
	rb->del_cls = cls;
}

int rb_size(struct rbtree *rb)
{
	return count_nodes(rb->root);
}

int rb_insert(struct rbtree *rb, void *key, void *data)
{
	rb->root = insert(rb, rb->root, key, data);
	rb->root->red = 0;
	return 0;
}

int rb_inserti(struct rbtree *rb, int key, void *data)
{
	rb->root = insert(rb, rb->root, INT2PTR(key), data);
	rb->root->red = 0;
	return 0;
}


int rb_delete(struct rbtree *rb, void *key)
{
	rb->root = delete(rb, rb->root, key);
	rb->root->red = 0;
	return 0;
}

int rb_deletei(struct rbtree *rb, int key)
{
	rb->root = delete(rb, rb->root, INT2PTR(key));
	rb->root->red = 0;
	return 0;
}


void *rb_find(struct rbtree *rb, void *key)
{
	struct rbnode *node = rb->root;

	while(node) {
		int cmp = rb->cmp(key, node->key);
		if(cmp == 0) {
			return node;
		}
		node = cmp < 0 ? node->left : node->right;
	}
	return 0;
}

void *rb_findi(struct rbtree *rb, int key)
{
	return rb_find(rb, INT2PTR(key));
}


void rb_foreach(struct rbtree *rb, void (*func)(struct rbnode*, void*), void *cls)
{
	traverse(rb->root, func, cls);
}


struct rbnode *rb_root(struct rbtree *rb)
{
	return rb->root;
}

void rb_begin(struct rbtree *rb)
{
	rb->rstack = 0;
	rb->iter = rb->root;
}

#define push(sp, x)		((x)->next = (sp), (sp) = (x))
#define pop(sp)			((sp) = (sp)->next)
#define top(sp)			(sp)

struct rbnode *rb_next(struct rbtree *rb)
{
	struct rbnode *res = 0;

	while(rb->rstack || rb->iter) {
		if(rb->iter) {
			push(rb->rstack, rb->iter);
			rb->iter = rb->iter->left;
		} else {
			rb->iter = top(rb->rstack);
			pop(rb->rstack);
			res = rb->iter;
			rb->iter = rb->iter->right;
			break;
		}
	}
	return res;
}

void *rb_node_key(struct rbnode *node)
{
	return node ? node->key : 0;
}

int rb_node_keyi(struct rbnode *node)
{
	return node ? PTR2INT(node->key) : 0;
}

void *rb_node_data(struct rbnode *node)
{
	return node ? node->data : 0;
}

static int cmpaddr(void *ap, void *bp)
{
	return ap < bp ? -1 : (ap > bp ? 1 : 0);
}

static int cmpint(void *ap, void *bp)
{
	return PTR2INT(ap) - PTR2INT(bp);
}


/* ---- left-leaning 2-3 red-black implementation ---- */

/* helper prototypes */
static int is_red(struct rbnode *tree);
static void color_flip(struct rbnode *tree);
static struct rbnode *rot_left(struct rbnode *a);
static struct rbnode *rot_right(struct rbnode *a);
static struct rbnode *find_min(struct rbnode *tree);
static struct rbnode *del_min(struct rbtree *rb, struct rbnode *tree);
/*static struct rbnode *move_red_right(struct rbnode *tree);*/
static struct rbnode *move_red_left(struct rbnode *tree);
static struct rbnode *fix_up(struct rbnode *tree);

static int count_nodes(struct rbnode *node)
{
	if(!node)
		return 0;

	return 1 + count_nodes(node->left) + count_nodes(node->right);
}

static void del_tree(struct rbnode *node, rb_del_func_t delfunc, void *cls)
{
	if(!node)
		return;

	del_tree(node->left, delfunc, cls);
	del_tree(node->right, delfunc, cls);

	delfunc(node, cls);
	free(node);
}

static struct rbnode *insert(struct rbtree *rb, struct rbnode *tree, void *key, void *data)
{
	int cmp;

	if(!tree) {
		struct rbnode *node = rb->alloc(sizeof *node);
		if(!node) {
			panic("failed to allocate tree node\n");
		}
		node->red = 1;
		node->key = key;
		node->data = data;
		node->left = node->right = 0;
		return node;
	}

	cmp = rb->cmp(key, tree->key);

	if(cmp < 0) {
		tree->left = insert(rb, tree->left, key, data);
	} else if(cmp > 0) {
		tree->right = insert(rb, tree->right, key, data);
	} else {
		tree->data = data;
	}

	/* fix right-leaning reds */
	if(is_red(tree->right)) {
		tree = rot_left(tree);
	}
	/* fix two reds in a row */
	if(is_red(tree->left) && is_red(tree->left->left)) {
		tree = rot_right(tree);
	}

	/* if 4-node, split it by color inversion */
	if(is_red(tree->left) && is_red(tree->right)) {
		color_flip(tree);
	}

	return tree;
}

static struct rbnode *delete(struct rbtree *rb, struct rbnode *tree, void *key)
{
	int cmp;

	if(!tree) {
		return 0;
	}

	cmp = rb->cmp(key, tree->key);

	if(cmp < 0) {
		if(!is_red(tree->left) && !is_red(tree->left->left)) {
			tree = move_red_left(tree);
		}
		tree->left = delete(rb, tree->left, key);
	} else {
		/* need reds on the right */
		if(is_red(tree->left)) {
			tree = rot_right(tree);
		}

		/* found it at the bottom (XXX what certifies left is null?) */
		if(cmp == 0 && !tree->right) {
			if(rb->del) {
				rb->del(tree, rb->del_cls);
			}
			rb->free(tree);
			return 0;
		}

		if(!is_red(tree->right) && !is_red(tree->right->left)) {
			tree = move_red_left(tree);
		}

		if(key == tree->key) {
			struct rbnode *rmin = find_min(tree->right);
			tree->key = rmin->key;
			tree->data = rmin->data;
			tree->right = del_min(rb, tree->right);
		} else {
			tree->right = delete(rb, tree->right, key);
		}
	}

	return fix_up(tree);
}

/*static struct rbnode *find(struct rbtree *rb, struct rbnode *node, void *key)
{
	int cmp;

	if(!node)
		return 0;

	if((cmp = rb->cmp(key, node->key)) == 0) {
		return node;
	}
	return find(rb, cmp < 0 ? node->left : node->right, key);
}*/

static void traverse(struct rbnode *node, void (*func)(struct rbnode*, void*), void *cls)
{
	if(!node)
		return;

	traverse(node->left, func, cls);
	func(node, cls);
	traverse(node->right, func, cls);
}

/* helpers */

static int is_red(struct rbnode *tree)
{
	return tree && tree->red;
}

static void color_flip(struct rbnode *tree)
{
	tree->red = !tree->red;
	tree->left->red = !tree->left->red;
	tree->right->red = !tree->right->red;
}

static struct rbnode *rot_left(struct rbnode *a)
{
	struct rbnode *b = a->right;
	a->right = b->left;
	b->left = a;
	b->red = a->red;
	a->red = 1;
	return b;
}

static struct rbnode *rot_right(struct rbnode *a)
{
	struct rbnode *b = a->left;
	a->left = b->right;
	b->right = a;
	b->red = a->red;
	a->red = 1;
	return b;
}

static struct rbnode *find_min(struct rbnode *tree)
{
	struct rbnode *node;

	if(!tree)
		return 0;

	while(node->left) {
		node = node->left;
	}
	return node;
}

static struct rbnode *del_min(struct rbtree *rb, struct rbnode *tree)
{
	if(!tree->left) {
		if(rb->del) {
			rb->del(tree->left, rb->del_cls);
		}
		rb->free(tree->left);
		return 0;
	}

	/* make sure we've got red (3/4-nodes) at the left side so we can delete at the bottom */
	if(!is_red(tree->left) && !is_red(tree->left->left)) {
		tree = move_red_left(tree);
	}
	tree->left = del_min(rb, tree->left);

	/* fix right-reds, red-reds, and split 4-nodes on the way up */
	return fix_up(tree);
}

#if 0
/* push a red link on this node to the right */
static struct rbnode *move_red_right(struct rbnode *tree)
{
	/* flipping it makes both children go red, so we have a red to the right */
	color_flip(tree);

	/* if after the flip we've got a red-red situation to the left, fix it */
	if(is_red(tree->left->left)) {
		tree = rot_right(tree);
		color_flip(tree);
	}
	return tree;
}
#endif

/* push a red link on this node to the left */
static struct rbnode *move_red_left(struct rbnode *tree)
{
	/* flipping it makes both children go red, so we have a red to the left */
	color_flip(tree);

	/* if after the flip we've got a red-red on the right-left, fix it */
	if(is_red(tree->right->left)) {
		tree->right = rot_right(tree->right);
		tree = rot_left(tree);
		color_flip(tree);
	}
	return tree;
}

static struct rbnode *fix_up(struct rbnode *tree)
{
	/* fix right-leaning */
	if(is_red(tree->right)) {
		tree = rot_left(tree);
	}
	/* change invalid red-red pairs into a proper 4-node */
	if(is_red(tree->left) && is_red(tree->left->left)) {
		tree = rot_right(tree);
	}
	/* split 4-nodes */
	if(is_red(tree->left) && is_red(tree->right)) {
		color_flip(tree);
	}
	return tree;
}

void rb_dbg_print_tree(struct rbtree *tree)
{
	struct rbnode *node;

	rb_begin(tree);
	while((node = rb_next(tree))) {
		printf("%d ", rb_node_keyi(node));
	}
	printf("\n");
}
