import math

def lat_lon_to_tile(lat, lon, z):
    n = 2.0 ** z

    # X-Kachel (Längengrad)
    # x = int((lon + 180.0) / 360.0 * n)
    x = (lon + 180.0) / 360.0 * n

    # Y-Kachel (Breitengrad unter Verwendung der Mercator-Projektion)
    lat_rad = math.radians(lat)
    # y = int((1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n)
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n

    # Einschränkung, falls die Koordinaten außerhalb der OSM-Grenzen liegen
    x = max(0, min(x, int(n) - 1))
    y = max(0, min(y, int(n) - 1))

    return x, y, z

