import math

def tile_to_lat_lon(x, y, z):
    n = 2.0 ** z
    lon_deg = x / n * 360.0 - 180.0

    # Latitude (Breitengrad) erfordert eine inverse Hyperbelfunktion
    lat_rad = math.atan(math.sinh(math.pi * (1.0 - 2.0 * y / n)))
    lat_deg = math.degrees(lat_rad)

    return lat_deg, lon_deg

