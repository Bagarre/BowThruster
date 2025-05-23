import cadquery as cq

# === Dimensions ===
board_length = 76.2
board_width = 50.8
standoff_height = 3.175
internal_height = 25.4
lid_thickness = 3.0
wall_thickness = 3.0

outer_length = board_length + 2 * wall_thickness
outer_width = board_width + 2 * wall_thickness
outer_height = standoff_height + internal_height + lid_thickness

# === Build outer shell ===
box = cq.Workplane("XY").box(outer_length, outer_width, outer_height)

# === Round outer edges ===
box = box.edges("|Z").fillet(3.0)
box = box.edges("#Z").fillet(1.5)

# === Hollow interior ===
interior = (
    cq.Workplane("XY")
    .workplane(offset=lid_thickness)
    .rect(board_length, board_width)
    .extrude(standoff_height + internal_height)
)
box = box.cut(interior)

# === Add standoffs ===
mount_offset = 3.5
post_diameter = 6.0
for x in [wall_thickness + mount_offset, outer_length - wall_thickness - mount_offset]:
    for y in [wall_thickness + mount_offset, outer_width - wall_thickness - mount_offset]:
        box = box.union(
            cq.Workplane("XY")
            .workplane(offset=lid_thickness)
            .center(x - outer_length / 2, y - outer_width / 2)
            .circle(post_diameter / 2)
            .extrude(standoff_height)
        )

# === Add wire slots ===
slot_width = 30
# Top and bottom sides (long edges)
for x_center in [-20, 20]:
    for y_sign in [-1, 1]:
        y_pos = (outer_width / 2 + y_sign * wall_thickness / 2) * y_sign
        slot = (
            cq.Workplane("XY")
            .workplane(offset=lid_thickness + standoff_height + internal_height / 2)
            .center(x_center, y_pos)
            .rect(slot_width, slot_height)
            .extrude(slot_depth * y_sign)
        )
        box = box.cut(slot)

# Right side (short edge, near pin 24/USB end)
for y_center in [-15, 15]:
    x_pos = outer_length / 2
    side_slot = (
        cq.Workplane("YZ")
        .workplane(offset=x_pos)
        .center(y_center, lid_thickness + standoff_height + internal_height / 2)
        .rect(slot_height, slot_width)
        .extrude(slot_depth)
    )
    box = box.cut(side_slot)

# === Export STL ===
cq.exporters.export(box, "bow_thruster_enclosure_final.stl")
print("✅ STL written: bow_thruster_enclosure_final.stl")
