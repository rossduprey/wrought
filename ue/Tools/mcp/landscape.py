# Copyright wrought. Editor MCP toolset — headless valley sculpt.
#
# Deployed to the launcher engine's EditorToolset plugin at
#   .../Toolsets/EditorToolset/Content/Python/editor_toolset/toolsets/landscape.py
# and registered by adding `from . import landscape` to that package's __init__.py.
# Kept here in the repo as the version-controlled source of truth (see
# planning/wrought-landscape-tooling.md in Cluster/knowledge).
#
# Sculpts the wrought valley as a TRUE UE Landscape, registered onto the sim's Place{x,y}
# frame: Place meters * 100 = UE cm (WroughtSimSubsystem: PlaneOrigin=0, UnitsPerMeter=100).
# The two authored deposits (core/geology.h): copper-hill at Place(0,0) r=40m (a raised
# hardrock mound); tin-creek at Place(300,120) r=25m (a placer, a water-cut channel).
#
# The river (2026-07-16): one continuous water-cut channel drains the valley west→east —
# skirting the copper hill's north flank, past the clay bank at Place(150,60) (the WEATHERED
# deposit sits ON the bank; the vat at its feet stays dry), through the tin placer at
# Place(300,120) (a placer IS river-worked ground), and out the east edge. Routed as a
# polyline; RIVER_WAYPOINTS_PLACE_M is the single source of truth — water actors are seated
# along the same points.

# Place-frame metres. Keep the vat/clay bank (150,60) >= ~1.5x half-width from the line.
RIVER_WAYPOINTS_PLACE_M = [
    (-80.0, 25.0),
    (-15.0, 85.0),
    (80.0, 92.0),
    (150.0, 88.0),
    (225.0, 100.0),
    (300.0, 120.0),
    (424.0, 150.0),
]

import base64
import math
import struct

import unreal

import toolset_registry


def _smooth_bump(t: float) -> float:
    """1 at t=0, smoothly to 0 at t>=1 (raised cosine)."""
    if t >= 1.0:
        return 0.0
    if t < 0.0:
        t = 0.0
    return 0.5 * (1.0 + math.cos(math.pi * t))


def _point_seg_dist(px: float, py: float,
                    ax: float, ay: float, bx: float, by: float) -> float:
    """Distance from point P to segment AB, all in the same units."""
    dx, dy = bx - ax, by - ay
    seg2 = dx * dx + dy * dy
    if seg2 <= 1e-9:
        return math.hypot(px - ax, py - ay)
    u = ((px - ax) * dx + (py - ay) * dy) / seg2
    u = max(0.0, min(1.0, u))
    cx, cy = ax + u * dx, ay + u * dy
    return math.hypot(px - cx, py - cy)


def _hash_noise(ix: int, iy: int) -> float:
    """Deterministic value noise in [-1, 1] from integer coords (no numpy)."""
    h = (ix * 374761393 + iy * 668265263) & 0xFFFFFFFF
    h = (h ^ (h >> 13)) * 1274126177 & 0xFFFFFFFF
    return (h / 0xFFFFFFFF) * 2.0 - 1.0


@unreal.uclass()
class LandscapeTools(unreal.ToolsetDefinition):
    """Headless landscape authoring for the wrought valley, sculpted onto the sim's
    Place{x,y} frame (Place meters * 100 = UE cm). Wraps the WroughtLandscape editor
    module (UWroughtLandscapeLibrary.CreateLandscapeFromHeightmapB64)."""

    @toolset_registry.tool_call
    @staticmethod
    def sculpt_valley(
        origin_x_m: float = -80.0,
        origin_y_m: float = -80.0,
        quads_x: int = 504,
        quads_y: int = 315,
        quads_per_section: int = 63,
        num_subsections: int = 1,
        meters_per_quad: float = 1.0,
        z_scale: float = 100.0,
        floor_m: float = 0.0,
        hill_peak_m: float = 13.0,
        hill_radius_m: float = 70.0,
        creek_depth_m: float = 3.5,
        creek_half_width_m: float = 22.0,
        creek_run_m: float = 140.0,
        undulation_amp_m: float = 1.6,
        noise_amp_m: float = 0.6,
        replace_existing: bool = True) -> str:
        """Sculpt and place the wrought valley as a true UE Landscape.

        The landscape's (0,0) corner is placed at UE (origin_x_m, origin_y_m) metres, so a
        vertex at local metre (lx, ly) sits at Place (origin_x_m+lx, origin_y_m+ly). Defaults
        span Place x in [-80, 424] m and y in [-80, 235] m, comfortably enclosing both
        deposits. A copper-hill mound rises at Place(0,0); a river channel is cut along
        RIVER_WAYPOINTS_PLACE_M (west edge -> hill's north skirt -> past the clay bank at
        Place(150,60) -> through the tin placer at Place(300,120) -> east edge); the rest
        is a gently undulating floor with fine noise.

        Args:
            origin_x_m: Place-x (metres) of the landscape's (0,0) corner.
            origin_y_m: Place-y (metres) of the landscape's (0,0) corner.
            quads_x: quad count in X (vertices = quads_x+1); must be a multiple of
                num_subsections*quads_per_section.
            quads_y: quad count in Y (vertices = quads_y+1); same divisibility rule.
            quads_per_section: quads per subsection (7/15/31/63/127/255).
            num_subsections: subsections per component (1 or 2).
            meters_per_quad: horizontal metres per quad (sets landscape XY scale = *100 cm).
            z_scale: landscape Z scale; world Z(cm) = (h-32768) * z_scale / 128.
            floor_m: baseline valley-floor height in metres.
            hill_peak_m: peak height of the copper-hill mound above floor, metres.
            hill_radius_m: influence radius of the hill skirt, metres.
            creek_depth_m: depth the river channel is cut below floor, metres.
            creek_half_width_m: half-width of the river channel cross-section, metres.
            creek_run_m: RETIRED (river is now the module-level polyline); ignored.
            undulation_amp_m: amplitude of long-wavelength ground undulation, metres.
            noise_amp_m: amplitude of fine per-vertex noise, metres.
            replace_existing: if true, destroy any prior 'WroughtValley' landscape first.

        Returns:
            A summary string (created actor path, dimensions, and height range).
        """
        size_x = quads_x + 1
        size_y = quads_y + 1
        comp_quads = num_subsections * quads_per_section
        if quads_x % comp_quads or quads_y % comp_quads:
            raise ValueError(
                f'quads_x/quads_y ({quads_x},{quads_y}) must be multiples of '
                f'num_subsections*quads_per_section ({comp_quads}).')

        # Deposit anchors in LOCAL metres (Place - origin).
        hill_lx, hill_ly = 0.0 - origin_x_m, 0.0 - origin_y_m

        # The river: a polyline in local metres (see RIVER_WAYPOINTS_PLACE_M).
        # creek_run_m is retired (kept in the signature for schema stability).
        pts = [(px - origin_x_m, py - origin_y_m) for px, py in RIVER_WAYPOINTS_PLACE_M]
        segs = list(zip(pts[:-1], pts[1:]))

        # Height -> uint16. World Z(cm) = (h-32768) * z_scale/128, so
        # h = 32768 + Z_m * 100 * 128 / z_scale.
        h_per_m = 100.0 * 128.0 / z_scale
        heights = bytearray(size_x * size_y * 2)
        hmin, hmax = 65535, 0
        mpq = meters_per_quad

        for j in range(size_y):
            ly = j * mpq
            row = j * size_x
            for i in range(size_x):
                lx = i * mpq

                z = floor_m
                # Copper hill: raised cosine mound.
                dh = math.hypot(lx - hill_lx, ly - hill_ly)
                z += hill_peak_m * _smooth_bump(dh / hill_radius_m)
                # The river: cut a channel (cosine cross-section) along the polyline.
                dc = min(_point_seg_dist(lx, ly, a[0], a[1], b[0], b[1])
                         for a, b in segs)
                z -= creek_depth_m * _smooth_bump(dc / creek_half_width_m)
                # Long-wavelength undulation + fine noise for a natural floor.
                z += undulation_amp_m * (
                    math.sin(lx * 0.012) * math.cos(ly * 0.017))
                z += noise_amp_m * _hash_noise(i, j)

                h = int(round(32768.0 + z * h_per_m))
                h = max(1, min(65534, h))
                if h < hmin:
                    hmin = h
                if h > hmax:
                    hmax = h
                struct.pack_into('<H', heights, (row + i) * 2, h)

        heights_b64 = base64.b64encode(bytes(heights)).decode('ascii')

        if replace_existing:
            LandscapeTools._destroy_existing('WroughtValley')

        location = unreal.Vector(origin_x_m * 100.0, origin_y_m * 100.0, 0.0)
        scale = unreal.Vector(mpq * 100.0, mpq * 100.0, z_scale)

        landscape = unreal.WroughtLandscapeLibrary.create_landscape_from_heightmap_b64(
            size_x, size_y, quads_per_section, num_subsections,
            location, scale, heights_b64)
        if landscape is None:
            raise RuntimeError(
                'CreateLandscapeFromHeightmapB64 returned None — see the editor log.')

        zmin_m = (hmin - 32768) / h_per_m
        zmax_m = (hmax - 32768) / h_per_m
        wp = ' -> '.join(f'({px:.0f},{py:.0f})' for px, py in RIVER_WAYPOINTS_PLACE_M)
        return (f'Sculpted {landscape.get_path_name()} — {size_x}x{size_y} verts, '
                f'{comp_quads}-quad components, origin Place({origin_x_m},{origin_y_m}) m, '
                f'height {zmin_m:.1f}..{zmax_m:.1f} m. '
                f'Copper hill at local ({hill_lx:.0f},{hill_ly:.0f}) m. '
                f'River (Place m): {wp}; depth {creek_depth_m} m, '
                f'half-width {creek_half_width_m} m.')

    @staticmethod
    def _destroy_existing(label: str) -> None:
        world = unreal.get_editor_subsystem(
            unreal.UnrealEditorSubsystem).get_editor_world()
        if world is None:
            return
        for actor in unreal.GameplayStatics.get_all_actors_of_class(
                world, unreal.Landscape):
            if actor.get_actor_label() == label:
                unreal.EditorActorSubsystem().destroy_actor(actor)
