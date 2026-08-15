otr = otr or {}
otr.talk = otr.talk or {}

function otr.talk.fary(text)
    otr.log("fary: " .. tostring(text))
    return Result.ok(true)
end
