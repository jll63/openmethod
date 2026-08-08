-- Substitute a marker in a doc comment with the contents of a file.
--
-- A paragraph whose entire text is
--
--     include:<path>[#<tag>[;<tag>...]]
--
-- is replaced by a code block holding <path>, or the named `// tag::name[]`
-- regions of it. The path is relative to `transform-options.include.root`,
-- itself relative to the directory holding this mrdocs.yml. Typical use:
--
--     //! @par Example
--     //! include:virtual_ptr.cpp#setup;assign_nullptr
--
-- The point is that the rendered snippet is a region of a file the build
-- compiles and runs, so a reference example cannot drift from the library.
--
-- Regions are selected in file order, the way Asciidoctor's `tags=` attribute
-- selects them, and each contiguous run is dedented on its own before the runs
-- are joined by a blank line. Per-run dedent is what lets a snippet draw its
-- setup from namespace scope and its body from inside a test case and still
-- render flush.
--
-- Why the whole block list is rebuilt rather than the marker patched in place:
-- three gaps in the 0.8.0 extension API, reported at
-- https://cpplang.slack.com/archives/C0508A7LWUV/p1785455605224149
--
--   * `doc.document[i] = block` fails with "attempt to index a userdata value"
--     -- the Lua binding exposes no __newindex for array proxies, so
--     DescribedArrayProxy::set is unreachable from a script.
--   * A proxy read out of the corpus is rejected as setter input ("expects an
--     object describing a polymorphic value"), so blocks cannot be handed back
--     verbatim; they have to be deep-copied into plain tables.
--   * `level` is refused by the generic setter, hence UNWRITABLE below.
--
-- If those are fixed upstream this whole file collapses to a few lines.

-- Fields the generic setter cannot write. `level` is a heading's depth; MrDocs
-- does not parse markdown `##` headings in doc comments, so heading blocks only
-- ever come from `@par` at level 1 -- which is the default -- and dropping it
-- round-trips.
local UNWRITABLE = { level = true }

local function dirname(path)
    return path:match("^(.*)/[^/]*$") or "."
end

local function is_array(value)
    local ok, n = pcall(function()
        return #value
    end)
    return ok and n and n > 0
end

local function copy(value)
    if type(value) ~= "userdata" then
        return value
    end

    if is_array(value) then
        local out = {}
        for i = 1, #value do
            out[i] = copy(value[i])
        end
        return out
    end

    local out, any = {}, false
    for key, field in pairs(value) do
        any = true
        if not UNWRITABLE[key] then
            out[key] = copy(field)
        end
    end

    -- An empty proxy is an absent optional, not an empty object.
    if not any then
        return nil
    end

    return out
end

-- Drop the common indentation of `lines`, then join them.
local function dedent(lines)
    local indent

    for _, line in ipairs(lines) do
        local lead = line:match("^([ \t]*)%S")
        if lead and (not indent or #lead < #indent) then
            indent = lead
        end
    end

    if indent and #indent > 0 then
        for i, line in ipairs(lines) do
            lines[i] = line:sub(#indent + 1)
        end
    end

    return (table.concat(lines, "\n"):gsub("%s+$", ""))
end

-- Return the regions of `text` covered by `tags`, in file order, or the whole
-- text when `tags` is nil. The second result lists the tags that never opened.
local function select_regions(text, tags)
    if not tags then
        return (text:gsub("%s+$", "")), {}
    end

    local wanted, found = {}, {}
    for _, tag in ipairs(tags) do
        wanted[tag] = true
    end

    local regions, current, depth = {}, nil, 0

    local function flush()
        if current then
            regions[#regions + 1] = dedent(current)
            current = nil
        end
    end

    for line in (text .. "\n"):gmatch("([^\n]*)\n") do
        local opens = line:match("tag::([%w_%-%.]+)%[%]")
        local closes = line:match("end::([%w_%-%.]+)%[%]")

        if opens then
            if wanted[opens] then
                found[opens] = true
                depth = depth + 1
            end
        elseif closes then
            if wanted[closes] then
                depth = depth - 1
                if depth == 0 then
                    flush()
                end
            end
        elseif depth > 0 then
            current = current or {}
            current[#current + 1] = line
        end
    end

    flush()

    local missing = {}
    for _, tag in ipairs(tags) do
        if not found[tag] then
            missing[#missing + 1] = tag
        end
    end

    return table.concat(regions, "\n\n"), missing
end

local function read_file(path)
    local file = io.open(path, "r")
    if not file then
        return nil
    end
    local text = file:read("*a")
    file:close()
    return text
end

-- `include:<path>` or `include:<path>#<tag>[;<tag>...]`, alone in a paragraph.
local function parse_marker(block)
    if block.kind ~= "paragraph" then
        return nil
    end

    local inlines = block.children
    if not inlines or #inlines ~= 1 or inlines[1].kind ~= "text" then
        return nil
    end

    local spec = inlines[1].literal:match("^include:(%S+)$")
    if not spec then
        return nil
    end

    local path, tail = spec:match("^([^#]+)#(.+)$")
    if not path then
        return spec, nil
    end

    local tags = {}
    for tag in tail:gmatch("[^;]+") do
        tags[#tags + 1] = tag
    end

    return path, tags
end

mrdocs.register_transform("include", function(ctx)
    local root = dirname(ctx.config.config) .. "/" .. (ctx.params.root or ".")
    local lang = ctx.params.lang or "cpp"

    for _, symbol in ipairs(ctx.corpus.symbols) do
        local document = symbol.doc and symbol.doc.document

        if document and #document > 0 then
            local blocks, substituted = {}, false

            for i = 1, #document do
                local block = document[i]
                local path, tags = parse_marker(block)

                if path then
                    local full = root .. "/" .. path
                    local text = read_file(full)
                    if not text then
                        error("include: cannot read " .. full)
                    end

                    local body, missing = select_regions(text, tags)
                    if #missing > 0 then
                        error(
                            "include: no tag " .. table.concat(missing, ", ")
                            .. " in " .. full)
                    end

                    blocks[i] = { kind = "code", literal = body, info = lang }
                    substituted = true
                else
                    blocks[i] = copy(block)
                end
            end

            if substituted then
                symbol.doc.document = blocks
            end
        end
    end
end)
