otr = otr or {}
otr.talk = otr.talk or {}

function otr.talk.merchant(text)
    otr.log("merchant stub: " .. tostring(text))
    return Result.ok(true)
end
