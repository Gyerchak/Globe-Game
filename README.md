# Projekt-001

Mosaic builder for Copernicus GLO‑30 DEM data.

- Reads 1°×1° tiles from a read‑only drive.
- Merges them into 20°×20° chunks (9 rows × 18 columns).
- Downscales each chunk to 1000×1000 px using Lanczos resampling.
- Outputs files named `{row}{col}.tif` (e.g., `0A.tif`, `4J.tif`, `8R.tif`).

## Build
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)