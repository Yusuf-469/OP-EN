# Sample data provenance

Both files are **self-created** for this assignment by `uew_cli make-samples data` (source:
`src/cli/sample_data.cpp`). No third-party geometry or simulation results are included. Licence: same as the
repository (original work by the candidate).

## `heat_sink.stl` - ASCII STL geometry

* Shape: finned heat sink. Base plate 60 x 40 x 6 mm with five 3 mm fins up to 36 mm, extruded along y.
* Units: **millimetres** (STL stores no units; select "millimetres" in the UI, the default).
* 64 triangles, 14.3 kB. Side walls from the extruded outline; end caps from non-overlapping rectangles.
* Export notes: equivalent to a CAD "Save as STL, ASCII, units mm" export. Facet normals are written from the
  right-hand rule of the vertex order; the viewer recomputes normals and renders both sides.

## `heat_sink_temperature_synthetic.csv` - nodal temperatures (SYNTHETIC)

* **Synthetic. This is not an ANSYS export** and was not produced by any FE solver.
* Schema follows the assignment: `node_id,x_m,y_m,z_m,temperature_C` (1065 nodes; coordinates in metres,
  temperature in °C), i.e. the shape of an ANSYS Mechanical "Export > nodal temperature" table after renaming the
  columns.
* Values: nodes on a grid over the same heat-sink geometry (5 slices in y). Base nodes:
  `T = Tb + 3 (1 - z/6 mm)` with `Tb = 82 - 6|x - 30 mm|/30 mm - 2 y/40 mm` °C. Fin nodes use the classical fin
  profile `T = 25 + (Tb - 25) cosh(m (Lf - s)) / cosh(m Lf)` with `m = 60 1/m`, `Lf = 30 mm`, `s` = height above the base.
  Range 40.8 to 85.0 °C.

## Test fixtures (`tests/fixtures/bad_*`)

Hand-written malformed files used by the integration and E2E tests: empty files, header only, wrong header,
NaN/Inf/text values, extra column, duplicate node id, binary STL, missing/NaN vertex, truncated STL, no facets.
