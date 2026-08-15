otr = otr or {}
otr.talk = otr.talk or {}

function otr.talk.oracle(text)
    otr.log("oracle stub: " .. tostring(text))
    return Result.ok(true)
end
