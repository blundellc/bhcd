#!/usr/bin/lua

require 'bhcd'

AdjMtx = {}
function AdjMtx:new(names)
    local obj = {}
    obj.names = names
    table.sort(obj.names)
    obj.num_elems = table.getn(names)
    obj._incoming = {}
    obj.num_outgoing = {}
    setmetatable(obj, self)
    self.__index = self
    return obj
end

function AdjMtx:vertices()
    local ii = 0
    local last = self.num_elems
    local names = self.names
    return function ()
            ii = ii + 1
            if ii <= last then
                return names[ii]
            end
        end
end

function AdjMtx:add(src, dst)
    if src == dst then
        return
    end
    local incoming = self._incoming[dst]
    if not incoming then
        incoming = {}
        self._incoming[dst] = incoming
    end
    if not incoming[src] then
        incoming[src] = true
        local num_out = self.num_outgoing[src] or 0
        self.num_outgoing[src] = num_out + 1
    end
end

function AdjMtx:incoming(uu)
    local incoming = self._incoming[uu]
    if incoming == nil then
        return function () return; end
    end
    local vv = nil
    return function ()
            vv = next(incoming, vv)
            if vv ~= nil then
                return vv
            end
        end
end

function AdjMtx:print()
    local max_len = string.len('src/dst')
    for uu in self:vertices() do
        local len = string.len(uu)
        if len > max_len then
            max_len = len
        end
    end
    local fmt = string.format('%%%us', max_len)
    local function atom(vv)
            io.write(string.format(fmt, vv), ' ')
        end

    atom('src/dst')
    for uu in self:vertices() do
        atom(uu)
    end
    io.write('\n')
    for src in self:vertices() do
        atom(src)
        for dst in self:vertices() do
            if self._incoming[dst] then
                atom(tostring(not not self._incoming[dst][src]))
            else
                atom(tostring(false))
            end
        end
        io.write('\n')
    end
    io.write('\n')

    for dst in self:vertices() do
        atom(dst)
        io.write(string.format('%2u ', self.num_outgoing[dst] or 0), ' ')
        for src in self:incoming(dst) do
            atom(src)
        end
        io.write('\n')
    end
    io.write('\n')
end

function load_train(fname)
    local dd = dataset.load(fname)
    local adj = AdjMtx:new(dd:labels())
    for _, elem in pairs(dd:elems()) do
        local src, dst = unpack(elem)
        if dd:get(src, dst) then
            adj:add(src, dst)
        end
    end
    return adj
end

function pagerank_tostring(pr)
    local sum = 0
    for k,v in pairs(pr) do
        sum = sum + v
    end
    local str = string.format("%2.2f:{", sum)
    for k,v in pairs(pr) do
        str = str .. string.format('%s:%f ', k, v)
    end
    str = str .. "}"
    return str
end

function pagerank(adj, max_iter, damp)
    local pr = {}
    for uu in adj:vertices() do
        pr[uu] = 1/adj.num_elems
    end
    local iter = 0
    while iter < max_iter do
        local newpr = {}
        local change = 0
        for uu in adj:vertices() do
            local sum = 0
            for vv in adj:incoming(uu) do
                sum = sum + pr[vv]/adj.num_outgoing[vv]
            end
            newpr[uu] = (1-damp)/adj.num_elems + damp*sum
            change = change + math.abs(pr[uu] - newpr[uu])
        end
        pr = newpr
        if change < 1e-3 then
            break
        end
        iter = iter+1
    end
    return pr
end

function pagerank_rooted(root, adj, max_iter, damp)
    local pr = {}
    for uu in adj:vertices() do
        pr[uu] = 1/adj.num_elems
    end
    local iter = 0
    while iter < max_iter do
        local newpr = {}
        local change = 0
        for uu in adj:vertices() do
            local sum = 0
            for vv in adj:incoming(uu) do
                sum = sum + pr[vv]/adj.num_outgoing[vv]
            end
            local restart = 0
            if uu == root then
                restart = 1-damp
            end
            newpr[uu] = restart + damp*sum
            change = change + math.abs(pr[uu] - newpr[uu])
        end
        pr = newpr
        if change < 1e-8 then
            break
        end
        iter = iter+1
    end
    return pr
end

function output_pred(pr, roots, test_data)
    for _, elem in pairs(test_data:elems()) do
        local src, dst = unpack(elem)
        local truth = test_data:get(src, dst)
        local pred = 0
        if roots[src][dst] > pr[dst] then
            pred = 1
        end
        local not_pred = 1-pred
        io.write(src,',',dst,',',tostring(truth),',',math.log(not_pred),',',math.log(pred),'\n')
    end
end

function main()
    if #arg < 2 then
        print(string.format('usage: %s train test', arg[0]))
        return
    end

    local train_fname = arg[1]
    local test_fname = arg[2]
    local adj = load_train(train_fname)
    --adj:print()
    local pr = pagerank(adj, 100, 0.85, 0.1)
    --print(pagerank_tostring(pr))
    local roots = {}
    for root in adj:vertices() do
        roots[root] = pagerank_rooted(root, adj, 1000, 0.85)
        --io.write(root,' ')
        --print(pagerank_tostring(roots[root]))
    end
    output_pred(pr, roots, dataset.load(test_fname))
end

main()
