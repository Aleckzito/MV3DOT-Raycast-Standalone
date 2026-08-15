-- Standalone otr namespace. C++ fills host.* bindings.
otr = otr or {}
otr.hooks = otr.hooks or {}

function otr.log(msg)
    if host ~= nil and host.log ~= nil then
        host.log(tostring(msg))
    else
        print("[luau] " .. tostring(msg))
    end
end
