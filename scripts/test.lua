ch = res:children()[1]
chch = ch:children()[1]
print('res', res)
print(res:logresp())
print('ch', ch)
print(ch:logresp())
print('chch', chch)
print(chch:logresp())
print(chch:is_leaf())
print(chch:label())
dd = dataset.load("data/blocks.gml")
print(dd)
for _, ll in pairs(dd:labels()) do print(ll); end
for _, pair in pairs(dd:elems()) do src, dst = unpack(pair); print(src, dst, dd:get(src, dst)); end
