module('fixture', package.seeall)

function object(bin)
    local f = io.open('test/fixtures/objects/' .. bin, 'r')
    if f == nil then
        error('test/fixtures/objects/' .. bin .. ' not exists')
    end
    local data = f:read('*a')
    f:close()
    return data
end

function request(bin)
    local f = io.open('test/fixtures/request/' .. bin, 'r')
    if not f then
        error('test/fixtures/request/' .. bin .. ' not exists')
    end
    local data = f:read('*a')
    f:close()
    return data
end
