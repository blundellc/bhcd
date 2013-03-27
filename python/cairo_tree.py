import cairo
import math

layout_debug = False

class layout_tree(object):
    def __init__(self, children=None, height=None):
        self._children = children
        if children is None:
            self._children = []
        else:
            self._children = list(self._children)

        # supplied height
        self.height = height

        # offset of top left corner
        self._offset = None
        # width and height of tree and descendants, from the offset
        self._dimensions = None
        # amount of height that belongs to this node only
        self._my_height = None
        # string label associated with leaf
        self._label = None

    @property
    def children(self):
        return self._children

    @property
    def label(self):
        return self._label

    def get_node_location(self):
        return (self._offset[0] + self._dimensions[0]/2.0
               ,self._offset[1] + self._dimensions[1])

    def get_width_mass(self):
        if not self._children:
            return [1.0]
        return [sum(ch.get_width_mass()) for ch in self._children]

    def get_height_mass(self):
        if self.height is None:
            return self.get_height_depth()
        return self.height

    def get_height_depth(self):
        if not self._children:
            return 1
        return 1 + max([ch.get_height_depth() for ch in self._children])

    def get_leaves(self):
        if not self._children:
            return [self]
        return [leaf for ch in self._children for leaf in ch.get_leaves()]

    def get_min_dimensions(self, ctx):
        bound_height = 0
        min_width = 0
        for leaf in self.get_leaves():
            leaf_width, leaf_height = leaf.get_bounding_box(ctx)
            bound_height = max(bound_height, leaf_height)
            min_width += leaf_width
        return min_width, bound_height

    def get_good_dimensions(self, ctx):
        min_width, min_height = self.get_min_dimensions(ctx)
        nleaves = len(self.get_leaves())
        width = (min_width/nleaves + 1)*nleaves
        depth = self.get_height_mass()
        height = min_height + depth*min_height/2.0
        return width, height

    def layout(self, ctx):
        """invoke on the root node to do the layout. ctx is the cairo context"""
        x1, y1, x2, y2 = ctx.clip_extents()
        width = x2-x1
        height = y2-y1

        min_width, bound_height = self.get_min_dimensions(ctx)

        if min_width > width:
            raise RuntimeError, 'not enough width in surface. need %f (got %f)' % (min_width, width)
        if bound_height > height:
            raise RuntimeError, 'not enough height in surface. need %f (got %f)' % (bound_height, height)

        for leaf in self.get_leaves():
            leaf.set_label_box_height(bound_height)

        # account for the space used by the labels when considering the height
        self.update_layout((x1, y1), (width, height-bound_height))

    def update_layout(self, offset, dimensions):
        self._offset = offset
        self._dimensions = dimensions

        wmass = self.get_width_mass()
        zw = sum(wmass)

        hdepth = self.get_height_mass()
        self._my_height = self._dimensions[1]/hdepth
        ch_height = self._dimensions[1]*(hdepth-1)/hdepth

        ox = self._offset[0]
        oy = self._offset[1] + self._my_height

        for i, ch in enumerate(self._children):
            ch_width = wmass[i]*self._dimensions[0]/zw
            ch.update_layout((ox, oy), (ch_width, ch_height))
            ox += ch_width

    def get_node_location(self):
        return (self._offset[0] + self._dimensions[0]/2.0
               ,self._offset[1] + self._my_height/2.0)

    def draw(self, ctx):
        if layout_debug:
            ctx.save()
            ctx.set_line_width(1.0)
            ctx.rectangle(self._offset[0], self._offset[1], self._dimensions[0],
                    self._dimensions[1])
            ctx.stroke()
            ctx.restore()
        for ch in self._children:
            ctx.move_to(*self.get_node_location())
            ctx.line_to(*ch.get_node_location())
            ctx.stroke()
            ch.draw(ctx)

    def render_pdf(self, fname):
        render_pdf([self], fname)

class draw_leaf(layout_tree):
    def __init__(self, label=None, **kwargs):
        super(draw_leaf, self).__init__(**kwargs)
        self._label = str(label)
        self._label_box_height = None

    def set_label_box_height(self, height):
        self._label_box_height = height

    def get_bounding_box(self, ctx):
        if self._label is None:
            # TODO: bump min_width by the smallest width of a leaf?
            return 0.0, 0.0
        xbear,ybear,label_width,label_height,xadv,yadv = ctx.text_extents(self._label)
        # swap them as we rotate by pi/2
        leaf_width, leaf_height = label_height, label_width
        return leaf_width, leaf_height

    def draw(self, ctx):
        if layout_debug:
            ctx.save()
            ctx.set_line_width(1.0)
            ctx.rectangle(self._offset[0], self._offset[1], self._dimensions[0],
                    self._dimensions[1])
            ctx.stroke()
            ctx.restore()
        if self._label is None:
            return
        leaf_width, leaf_height = self.get_bounding_box(ctx)
        x = self._offset[0] + (self._dimensions[0] - leaf_width)/2.0
        y = self._offset[1] + self._dimensions[1] 
        ctx.save()
        ctx.move_to(x,y)
        ctx.rotate(math.pi/2)
        ctx.show_text(self._label)
        ctx.restore()

class draw_node(layout_tree):
    pass

def render_pdf(forest, fname, names=None):
    surf, ctx = init(fname)
    render_page(forest, surf, ctx, names)

def init(fname):
    surf = cairo.PDFSurface(fname, 100, 100)
    ctx = cairo.Context(surf)
    return surf, ctx
 
def render_page(forest, surf, ctx, names=None):
    if names is None:
        names = [None for tree in forest]
    offset_x, width, height = get_extents(forest, ctx)
    surf.set_size(sum(width), max(height))
    ctx.set_source_rgb(1.0, 1.0, 1.0)
    ctx.paint()
    ctx.set_source_rgb(0.0, 0.0, 0.0)
    for ox, w, h, name, tree in zip(offset_x, width, height, names, forest):
        ctx.save()
        ctx.rectangle(ox, 0, w, h)
        ctx.clip()
        if name is not None:
            xbear,ybear,name_width,name_height,xadv,yadv = ctx.text_extents(name)
            ctx.move_to(ox, name_height)
            ctx.show_text(name)
        tree.layout(ctx)
        tree.draw(ctx)
        ctx.restore()
    ctx.show_page()

def get_extents(forest, ctx):
    width = []
    height = []
    offset_x = []
    for tree in forest:
        tree_width, tree_height = tree.get_good_dimensions(ctx)
        offset_x.append(sum(width))
        width.append(tree_width)
        height.append(tree_height)
    return offset_x, width, height

def test():
    tree = draw_node(
            [draw_leaf('hello', height=0.3)
            ,draw_node([draw_leaf('world', height=0.4)
                       ,draw_leaf('!')], height=0.5)], height=0.6)

    tree.render_pdf('plot.pdf')

