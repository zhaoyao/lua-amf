-- vim foldmethod=marker, foldmarker={,}

require 'test.fixture'
local amf = require 'amf_codec'

local object_fixture = fixture.object
local request_fixture = fixture.request

local function assert_encoded(ver, o, bin)
    local fixture = object_fixture(bin)
    local buf = amf.encode(ver, o)
    if fixture ~= buf then
        local tmpf = io.open("/tmp/" .. bin, 'w')
        tmpf:write(buf)
        tmpf:close()
    end
    assert.equals(buf, fixture)
end

describe('amf0', function()

    it('should serialize booleans', function() 
        assert_encoded(0, true, 'amf0-boolean.bin')
    end)

    it('should serialize nils', function() 
        assert_encoded(0, nil, 'amf0-null.bin')
    end)

    it('should serialize numbers', function()    
        assert_encoded(0, 3.5, 'amf0-number.bin')
    end)

    it('should serialize strings', function()    
        assert_encoded(0, "this is a テスト", 'amf0-string.bin')
    end)

    it('should serialize dense tables', function()    
        assert_encoded(0, {'a', 'b', 'c', 'd'}, 'amf0-strict-array.bin')
    end)

    it('should serialize references', function()    
        -- only luajit passes, cause the field order is different in lua-5.1
        local obj = {foo='baz'; bar=3.14}
        local oo = {[0]=obj; [1]=obj}
        assert_encoded(0, oo, 'amf0-ref-test.bin')
    end)



end)

describe('amf3', function() 
    it('should serialize nils', function() 
        assert_encoded(3, nil, 'amf3-null.bin')
    end)

    it('should serialize true and false', function() 
        assert_encoded(3, true, 'amf3-true.bin')
        assert_encoded(3, false, 'amf3-false.bin')
    end)

    it('should serialize integers', function() 
        assert_encoded(3, amf.MAX_INT, 'amf3-max.bin')
        assert_encoded(3, amf.MIN_INT, 'amf3-min.bin')
        assert_encoded(3, 0, 'amf3-0.bin')
    end)

    it('should serialize large integers', function() 
        assert_encoded(3, amf.MAX_INT + 1, 'amf3-large-max.bin')
        assert_encoded(3, amf.MIN_INT - 1, 'amf3-large-min.bin')
    end)

    it('should serialize floats', function() 
        assert_encoded(3, 3.5, 'amf3-float.bin')
    end)

    it('should serialize big number as floats', function() 
        local bi = math.pow(2, 1000)
        assert_encoded(3, bi, 'amf3-bigNum.bin')
    end)

    it('should serialize strings', function() 
        assert_encoded(3, "String . String", 'amf3-string.bin')
    end)

    -- lua_next(L, idx) may return diferent key order, the test may fail 
    it('should serialize table', function() 
        assert_encoded(3, {answer=42; foo='bar'}, 'amf3-hash.bin')
    end)

    it('should serialize array', function() 
        assert_encoded(3, {1, 2, 3, 4}, 'amf3-strict-array.bin')
    end)

    it('should serialize empty table as null', function()
        assert_encoded(3, {}, 'amf3-null.bin')
    end)

    it('should serialize an array of mixed objects', function() 
        local h1 = {foo_one='bar_one'}
        local h2 = {foo_two=''}
        local so1 = {foo_three=42}

        input = {h1, h2, so1, {}, {h1, h2, so1}, {}, 42, '', {}, '', 'bar_one', so1}
        assert_encoded(3, input, 'amf3-mixed-array.bin')
    end)

    it('should keep references of duplicate strings', function() 
        local foo = 'foo'
        local bar = 'str'
        local sc = {str=foo}

        assert_encoded(3, {foo,bar,foo,bar,foo,sc}, 'amf3-string-ref.bin')
    end)

    it('should not reference the empty string', function() 
        assert_encoded(3, {'', ''}, 'amf3-empty-string-ref.bin')
    end)

    it('should keep reference of duplicate objects', function() 
        local obj1 = {foo='bar'}
        local obj2 = {foo=obj1.foo}

        assert_encoded(3, {{obj1,obj2}, 'bar', {obj1, obj2}}, 'amf3-object-ref.bin')
    end)


    it('should keep reference of duplicate arrays', function() 
        local a = {1, 2, 3}
        local b = {'a', 'b', 'c'}

        assert_encoded(3, {a, b, a, b}, 'amf3-array-ref.bin')
    end)

    it('should serialize a deep object graph with circular references', function() 
        local c1 = {}
        local c2 = {}
        local p = {}

        p.children = {c1, c2}
        c1.parent = p
        c2.parent = p

        assert_encoded(3, p, 'amf3-graph-member.bin')
    end)
end)

