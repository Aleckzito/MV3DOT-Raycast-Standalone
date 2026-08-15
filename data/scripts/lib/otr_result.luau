-- Pure Result helpers. Host-loaded; no require/dofile.
Result = Result or {}

function Result.ok(value)
    return { ok = true, value = value }
end

function Result.fail(code, message)
    return { ok = false, code = tostring(code or "error"), message = tostring(message or "") }
end

function Result.isOk(result)
    return type(result) == "table" and result.ok == true
end
