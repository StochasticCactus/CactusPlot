# Annotations and regions

## Drawing annotations

The drawing tools are in the button column and in **View ▸ Tools**:

| Tool | Key | How |
|---|---|---|
| **Text** | T | Click where the text should start, type the text in the box that opens (with `$math$` if you like) and click **OK**. |
| **Line** | L | Drag from one end to the other. |
| **Arrow** | A | Drag from the tail to the head. |
| **Box** | B | Drag from one corner to the opposite corner. |
| **Ellipse** | E | Drag the rectangle that holds the ellipse. |

The tool stays active, so you can draw several annotations in a row. Press **Esc** (or **V**) to
return to the **Select** tool, and then:

- **drag** an annotation to move it, or nudge it with the arrow keys (**Shift** for bigger steps);
- **double-click** it to edit its text, colours, arrow heads and position in the Appearance window;
- press **Delete**, or right-click ▸ **Delete annotation**, to remove it;
- use the Explorer to duplicate, hide or show annotations.

### World or viewport?

Every annotation has **Coordinates**:

- **World (moves with data):** the annotation is pinned to data values. An arrow pointing at a peak
  keeps pointing at it when you zoom or pan. Use this for labels of features in the data.
- **Viewport (fixed on page):** the annotation stays at the same place on the page. Use this for
  panel letters like "(a)" and for notes.

Annotations drawn with the mouse start in viewport coordinates. Switch them to world coordinates in
the Appearance window if they should follow the data.

### Annotations in commands

Grace's syntax works: define the object, then end it with `def`.

```
with string
    string on
    string loctype view
    string 0.05, 0.95
    string char size 1.4
    string def "(a)"

with line
    line on
    line loctype world
    line g0
    line 2.5, 0.8, 3.1, 0.45
    line arrow 2
    line color "tomato"
line def
```

`box` and `ellipse` work the same way, with two corners: `box 1, 1, 2, 2`.

## Regions

A region is an area of a graph. It is used to pick points: for example to restrict a set to the
points inside it, or to delete them. There are five regions, **R0** to **R4**.

### Drawing a polygon region

1. Choose the **Region** tool (**R**), or **Data ▸ Regions ▸ Draw Polygon Region**.
2. Click the corners of the polygon in a graph.
3. Double-click to close it. The locator reports which region was defined.

**Data ▸ Regions ▸ Show Regions** shows them on the plot (they are never exported);
**Clear Regions** removes them all.

### Other region types

Grace's other region types are available as commands. A region is a half-plane or band bounded by a line:

| Type | Points inside |
|---|---|
| `polyi`, `polyo` | inside, or outside, a polygon |
| `above`, `below` | above, or below, a line |
| `left`, `right` | left, or right, of a line |
| `horizi`, `horizo` | inside, or outside, a horizontal band |
| `verti`, `verto` | inside, or outside, a vertical band |

```
r1 type above
r1 line 0, 0.5, 10, 0.5      # the line from (0, 0.5) to (10, 0.5)
r1 on
r1 link to g0
```

### Using regions

- **Data ▸ Transformations ▸ Restrict:** choose a region (R0–R4) to keep only the points inside it.
  Tick **Keep points outside the region** to keep the others instead.
- In commands: `restrict(s0, r0)` keeps points inside R0; `restrict(s0, r0, on)` keeps those outside.

A condition works as well as a region: `restrict(s0, s0.x > 2 && s0.y < 5)`.
