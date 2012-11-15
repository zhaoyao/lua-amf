require 'test.fixture'

local amf = require 'amf'
local object_fixture = fixture.object
local request_fixture = fixture.request

local decode_amf = amf.decode

local function assert_eql(expected, actual)
    if type(expected) == 'table' then
        assert.equals('table', type(actual))
        for k, v in pairs(actual) do
            assert_eql(expected[k], v)
        end
    else
        assert.equals(expected, actual)
    end
end

local function assert_decoded(ver, bin, obj)
    local fixture = object_fixture(bin)
    local ret, err, pos = decode_amf(ver, fixture)
    assert.equals(nil, err)
    assert_eql(obj, ret)
    assert.equals(#fixture, pos)
end

---- boolean
describe('amf0', function()
    it('should decode boolean', function() 
        assert_decoded(0, 'amf0-boolean.bin', true)
    end)

    ---- nil
    it('should decode null', function() 
        assert_decoded(0, 'amf0-null.bin', nil)
        assert_decoded(0, 'amf0-undefined.bin', nil)
    end)

    ---- numbers
    it('should decode number', function() 
        assert_decoded(0, 'amf0-number.bin', 3.5)
    end)

    ---- string
    it('should decode string', function()
        assert_decoded(0, 'amf0-string.bin', 'this is a テスト')
    end)

    ---- table
    it('should decode hash', function()
        assert_decoded(0, 'amf0-hash.bin', {a='b'; c='d'})
    end)

    ---- ecma array
    it('should decode orinal array', function()
        assert_decoded(0, 'amf0-ecma-ordinal-array.bin', {['0']='a'; ['1']='b'; ['2']='c'; ['3']='d'})
    end)

    ---- strict array
    it('should decode orinal array', function()
        assert_decoded(0, 'amf0-strict-array.bin', {'a','b', 'c','d'})
    end)

    ---- anonymous objects
    it('should decode orinal array', function()
        assert_decoded(0, 'amf0-object.bin', {foo='baz'; bar=3.14})
    end)

    ---- typed object
    it('should decode orinal array', function()
        ret, err = decode_amf(0, object_fixture('amf0-typed-object.bin'))
        assert.equals(nil, err)
        assert_eql({foo='bar', __amf_alias__ = 'org.amf.ASClass'}, ret)
    end)

    ---- reference
    it('should decode orinal array', function()
        ret, err = decode_amf(0, object_fixture('amf0-ref-test.bin'))
        assert.equals(nil, err)
        assert.equals('table', type(ret))
        assert.is_true(ret['0'] ~= nil)
        assert.is_true(ret['0'] == ret['1']) 
    end)
end)

describe('amf3', function() 

    it("should deserialize a null", function()
        assert_decoded(3, 'amf3-null.bin', nil)
    end)

    it('should deserialize a false', function() 
        assert_decoded(3, 'amf3-false.bin', false)
    end)

    it('should deserialize a true', function() 
        assert_decoded(3, 'amf3-true.bin', true)
    end)

    it("should deserialize integers", function() 
        assert_decoded(3, 'amf3-max.bin', amf.MAX_INT)
        assert_decoded(3, 'amf3-0.bin', 0)
        assert_decoded(3, 'amf3-min.bin', amf.MIN_INT)
    end)

    it("should deserialize large integers", function() 
        assert_decoded(3, 'amf3-large-max.bin', amf.MAX_INT+1)
        assert_decoded(3, 'amf3-large-min.bin', amf.MIN_INT-1)
    end)

    it("should deserialize big nums", function() 
        assert_decoded(3, 'amf3-bignum.bin', math.pow(2, 1000))
    end)

    it("should deserialize simple string", function() 
        assert_decoded(3, 'amf3-string.bin', 'String . String')
    end)

    it("should deserialize date as epoch", function() 
        assert_decoded(3, 'amf3-date.bin', 0)
    end)

    it("should deserialize xml as string", function() 
        assert_decoded(3, 'amf3-xml-doc.bin', '<parent><child prop="test" /></parent>')
        assert_decoded(3, 'amf3-xml.bin', '<parent><child prop="test"/></parent>')
    end)

    it('should deserialize an array of mixed objects', function() 
        local h1 = {foo_one='bar_one'}
        local h2 = {foo_two=''}
        local so1 = {foo_three=42}
        local input = {h1, h2, so1, nil, {h1, h2, so1}, nil, 42, '', nil, '', 'bar_one', so1}
        assert_decoded(3, 'amf3-mixed-array.bin', input)
    end)

    it('should deserialize an byte array as string', function()

        local function c(...)
            t = {}
            for _, v in ipairs({...}) do
                table.insert(t, string.char(v))
            end
            return table.concat(t)
        end

        assert_decoded(3, 'amf3-byte-array.bin', c(0x00, 0x03, 0xe3, 0x81, 0x93, 0xe3, 0x82, 0x8c, 0x74, 0x65, 0x73, 0x74, 0x40))
    end)

    it('should keep references of duplicate strings', function() 
        local foo = 'foo'
        local bar = 'str'
        assert_decoded(3, 'amf3-string-ref.bin', {foo, bar, foo, bar, foo, {str='foo'}})
    end)

    it('should keep reference of duplicate objects', function() 
        local obj1 = {foo='bar'}
        local obj2 = {foo=obj1.foo}

        assert_decoded(3, 'amf3-object-ref.bin', {{obj1, obj2}, 'bar', {obj1, obj2}})
    end)

    it("should keep reference of duplicate object traits", function() 
        assert_decoded(3, 'amf3-trait-ref.bin', {{foo='foo'}, {foo='bar'}})
    end)

    it("should keep references of duplicate arrays", function() 
        local a = {1, 2, 3}
        local b = {'a', 'b', 'c'}
        assert_decoded(3, 'amf3-array-ref.bin', {a, b, a, b})
    end)

    it("should keep references of duplicate XML and XMLDocuments", function() 
        local x1 = '<parent><child prop="test"/></parent>'
        local x2 = '<parent><child prop="test"/></parent>'
        assert_decoded(3, 'amf3-xml-ref.bin', {x1, x2})
    end)

    it("should keep references of duplicate byte arrays", function() 
        assert_decoded(3, 'amf3-byte-array-ref.bin', {'ASDF', 'ASDF'})
    end)

    it("should deserialize a deep object graph with circular references", function() 
        local output, err, pos = amf.decode(3, object_fixture('amf3-graph-member.bin'))
        assert.equals(output, output.children[1].parent)
        assert.equals(nil, output.parent)
        assert.equals(2, #output.children)
    end)
end)
