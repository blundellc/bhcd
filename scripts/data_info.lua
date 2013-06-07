#!/usr/bin/lua

require 'bhcd'

function ls(tbl)
    for kk, vv in pairs(tbl) do
        print(kk, vv)
    end
end

local dd = dataset.load(arg[1])

local n0 = 0
local n1 = 0
local num_labels = table.getn(dd:labels())
local num_elems = table.getn(dd:elems())
local n00 = 0
local n10 = 0
local n11 = 0

for _, elem in pairs(dd:elems()) do
    local src, dst = unpack(elem)
    if dd:get(src, dst) then
        n1 = n1 + 1
    else
        n0 = n0 + 1
    end
end

local seen_so_far = 0
for _, src in pairs(dd:labels()) do
    local num_incident = 0
    for _, dst in pairs(dd:labels()) do
        if dst < src then
            num_incident = num_incident + 1
        end
    end
    local total_possible = (num_labels-seen_so_far)*(num_labels-seen_so_far+1)/2
    n00 = n00 + total_possible - num_incident
    n11 = n11 + num_incident
    if num_incident == 1 then
        n10 = n10 + 1
    end
end

print('num labels', num_labels)
print('num pairs',  num_labels*(num_labels+1)/2)
print('num elems',  num_elems)
print('num zeros',  n0)
print('num ones',   n1)
print('num 11', n11)
print('num 00', n00)
print('num 10', n10)

print(n0/num_elems)
print(n1/num_elems)
