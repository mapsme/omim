import urllib.request
import urllib.parse
import argparse
import json

def remove_key_arr(a, key):
  for i in a:
    if (isinstance(i, dict)):
      remove_key_dict(i, key)
      continue

    if (isinstance(i, list)):
      remove_key_arr(i, key)

def remove_key_dict(d, key):
  kv = []
  for k, v in d.items():
    kv.append((k, v))
  for k, v in kv:
    if (isinstance(d[k], dict)):
      remove_key_dict(d[k], key)
      continue

    if (isinstance(d[k], list)):
      remove_key_arr(d[k], key)
      continue

    if k == key:
      d.pop(k, None)

parser = argparse.ArgumentParser()
parser.add_argument("--google_token", help="", type=str)
parser.add_argument("--start_lat_lon", help="start lat, lon in format: lat,lon", type=str)
parser.add_argument("--end_lat_lon", help="end lat, lon in format: lat,lon", type=str)

args = parser.parse_args()

start_lat, start_lon = args.start_lat_lon.split(",")
end_lat, end_lon = args.end_lat_lon.split(",")

start_lat = float(start_lat)
start_lon = float(start_lon)
end_lat = float(end_lat)
end_lon = float(end_lon)

origin_str = '&origin=' + str(start_lat) + ',' + str(start_lon)
destination_str = '&destination=' + str(end_lat) + ',' + str(end_lon)
url = 'https://maps.googleapis.com/maps/api/directions/json?' + origin_str + destination_str + '&key=' + args.google_token + "&alternatives=true"
f = urllib.request.urlopen(url)

res = json.loads(f.read().decode('utf-8'))
remove_key_dict(res, "points")
remove_key_dict(res, "html_instructions")

#print(json.dumps(res, indent=2, sort_keys=True))
#print("routes len:", len(res['routes']))
#print("legs len:", len(res["routes"][0]['legs']))
points_all = []
for route in res['routes']:
  points_data = route["legs"][0]["steps"]
  points = []
  for item in points_data:
    #print(item)
    #print("")
    add_end = False
    if len(points) == 0:
      add_end = True

    end_lat = item['end_location']['lat']
    end_lon = item['end_location']['lng']

    start_lat = item['start_location']['lat']
    start_lon = item['start_location']['lng']

    if add_end:
      points.append({'lat':end_lat, 'lon':end_lon})

    points.append({'lat':start_lat, 'lon':start_lon})
  points_all.append(points)

#print(points)
for points in points_all:
  for point in points:
    print(point['lat'], point['lon'])
  print("END")
