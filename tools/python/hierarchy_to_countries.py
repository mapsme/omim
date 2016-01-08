#!/usr/bin/python
import sys, json, re
import os.path

class CountryDict(dict):
  def __init__(self, *args, **kwargs):
    dict.__init__(self, *args, **kwargs)
    self.order = ['id', 'v', 'c', 's', 'g']

  def __iter__(self):
    for key in self.order:
      if key in self:
        yield key
    for key in dict.__iter__(self):
      if key not in self.order:
        yield key

  def iteritems(self):
    for key in self.__iter__():
      yield (key, self.__getitem__(key))

def get_size(path, name):
  if path == '0':
    return 0
  filename = os.path.join(path, '{0}.mwm'.format(name))
  try:
    return os.path.getsize(filename)
  except OSError:
    sys.stderr.write('Missing file: {0}\n'.format(filename))
    return -1

def collapse_single(root):
  for i in range(len(root['g'])):
    if 'g' in root['g'][i]:
      if len(root['g'][i]['g']) == 1:
        # replace group by a leaf
        if 'c' in root['g'][i]:
          root['g'][i]['g'][0]['c'] = root['g'][i]['c']
        root['g'][i] = root['g'][i]['g'][0]
      else:
        collapse_single(root['g'][i])

if len(sys.argv) < 2:
  print 'Usage: {0} {{<path_to_mwm>|0}} [<hierarchy.txt>]'.format(sys.argv[0])
  sys.exit(1)

oldvs = {}
try:
  ovnpath = '.' if len(sys.argv) < 2 else os.path.dirname(sys.argv[2])
  with open(os.path.join(ovnpath, 'old_vs_new.csv'), 'r') as f:
    for line in f:
      m = re.match(r'(.+?)\t(.+)', line.strip())
      if m:
        if m.group(2) in oldvs:
          oldvs[m.group(2)].append(m.group(1))
        else:
          oldvs[m.group(2)] = [m.group(1)]
except IOError:
  pass

mwmpath = sys.argv[1]
filename = 'hierarchy.txt' if len(sys.argv) < 3 else sys.argv[2]
stack = [CountryDict({ "v": 151231, "id": "Countries", "g": [] })]
last = None
with open(filename, 'r') as f:
  for line in f:
    m = re.match('( *)(.+?)\n', line)
    if m:
      depth = len(m.group(1))
      if last is not None:
        lastd = last['d']
        del last['d']
        if lastd < depth:
          # last is a group
          last['g'] = []
          stack.append(last)
        else:
          last['s'] = get_size(mwmpath, last['id'])
          if last['s'] >= 0:
            stack[-1]['g'].append(last)
      while depth < len(stack) - 1:
        # group ended, add it to higher group
        g = stack.pop()
        stack[-1]['g'].append(g)
      items = m.group(2).split(';')
      last = CountryDict({ "id": items[0], "d": depth })
      if items[0] in oldvs:
        last['old'] = oldvs[items[0]]
      #if len(items) > 2:
      #  last['c'] = items[2]

# the last line is always a file
del last['d']
last['s'] = get_size(mwmpath, last['id'])
if last['s'] >= 0:
  stack[-1]['g'].append(last)
while len(stack) > 1:
  g = stack.pop()
  stack[-1]['g'].append(g)

collapse_single(stack[-1])
print json.dumps(stack[-1], indent=1)
