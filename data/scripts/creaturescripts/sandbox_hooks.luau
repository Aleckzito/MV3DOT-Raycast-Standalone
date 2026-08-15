otr = otr or {}
otr.hooks = otr.hooks or {}

function otr.hooks.onKill(ctx)
    otr.log("onKill kind=" .. tostring(ctx and ctx.kind))
end

function otr.hooks.onFire(ctx)
    otr.log("onFire")
end

function otr.hooks.onHunter(ctx)
    otr.log("onHunter")
end

function otr.hooks.onLockOn(ctx)
    otr.log("onLockOn kind=" .. tostring(ctx and ctx.kind))
end

function otr.hooks.onLoot(ctx)
    otr.log("onLoot")
end

function otr.hooks.onQuest(ctx)
    otr.log("onQuest kind=" .. tostring(ctx and ctx.kind))
end

function otr.hooks.onVocation(ctx)
    otr.log("onVocation kind=" .. tostring(ctx and ctx.kind))
end
