from lat_lon_to_tile import lat_lon_to_tile
from tile_to_lat_lon import tile_to_lat_lon

# Example for conversion from lat and lon to tile coordinates
print("Example for conversion from lat and lon to tile coordinates")
lat = 49.78327
lon = 12.08161
x, y, z = lat_lon_to_tile(lat, lon, 16)
print(f"Input Latitude: {lat}, Longitude: {lon}, Zoom: {z}")
print(f"Output Zoom: {z}, X: {x}, Y: {y}")
print("")

# Example for conversion from tile coordinates to lat and lon
print("Example for conversion from tile coordinates to lat and lon")
x = 34850
y = 22822
z = 16
lat, lon = tile_to_lat_lon(x, y, z)
print(f"Intput Zoom: {z}, X: {x}, Y: {y}")
print(f"Output Latitude and Longitude: {lat}, {lon}")
print("")
