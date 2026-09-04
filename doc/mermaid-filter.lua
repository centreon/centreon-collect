--[[ Render mermaid code blocks as images, for the writers that cannot do it
     themselves -- LaTeX/PDF chief among them.

     Each block is rendered by mmdc into doc/.mermaid-cache, named after the
     sha1 of its source: a second run re-renders nothing, which matters with
     41 diagrams. Delete the directory to force a full re-render.

     PNG rather than SVG: xelatex needs --shell-escape and inkscape to accept
     an SVG, and that is a lot of moving parts for a document one just wants
     to read. Scale 3 keeps the text sharp when zoomed. ]]

local cache = '.mermaid-cache'
-- Which binary renders: `mmdc` if installed globally, `npx mmdc` for a local
-- install, or a podman wrapper. Overridable so the filter does not care.
local mmdc = os.getenv('MMDC') or 'mmdc'

local function file_exists(path)
  local f = io.open(path, 'r')
  if f then f:close() return true end
  return false
end

function CodeBlock(block)
  if not block.classes:includes('mermaid') then
    return nil
  end

  os.execute('mkdir -p ' .. cache)
  local digest = pandoc.utils.sha1(block.text)
  local src = cache .. '/' .. digest .. '.mmd'
  local img = cache .. '/' .. digest .. '.png'

  if not file_exists(img) then
    local f = io.open(src, 'w')
    if not f then
      io.stderr:write('mermaid: cannot write ' .. src .. '\n')
      return nil
    end
    f:write(block.text)
    f:close()
    -- -b white: the default is transparent, which turns black text into an
    -- invisible smear on a white page.
    local cmd = string.format(
      '%s -i %s -o %s -b white -s 3 -q 2>/dev/null', mmdc, src, img)
    if not os.execute(cmd) or not file_exists(img) then
      io.stderr:write('mermaid: mmdc failed on ' .. src ..
                      ' -- leaving the block as code\n')
      return nil
    end
  end

  -- 95% of the text width, so a wide diagram shrinks instead of running off
  -- the page, and a narrow one is not blown up past its natural size.
  local image = pandoc.Image({}, img, '', pandoc.Attr('', {}, {
    {'width', '95%'}
  }))
  return pandoc.Para({image})
end
