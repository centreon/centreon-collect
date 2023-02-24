# Contribute to the Centreon Stream Connectors project

## How to contribute

There are many ways you can contribute to this project and everyone should be able to help in its own way.

### For code lovers

You can work on Stream Connectors

- Create a new stream connector
- Update an existing stream connector
- [Fix issues](https://github.com/centreon/centreon-stream-connector-scripts/issues)

<<<<<<< HEAD
You can improve our Lua modules
=======
You can improve our Lua modules 
>>>>>>> centreon-stream-connector-scripts/feat(streamconnector)-add-refacto-bsm-v2-lua-files

- Add a new module
  - Comment it
  - Document it
  - *optional* Provide usage examples
- Update an existing module
  - Update the documentation (if it changes the input and/or output of a method)
  - Update usage examples if there are any and if they are impacted by the change

<<<<<<< HEAD
### For everybody
=======
### For everybody 
>>>>>>> centreon-stream-connector-scripts/feat(streamconnector)-add-refacto-bsm-v2-lua-files

Since we are not all found of code, there are still ways to be part of this project

- Open issues for bugs or feedbacks (or help people)
- Update an already existing example or provide new ones

## Code guidelines

If you want to work on our LUA modules, you must follow the coding style provided by luarocks
[Coding style guidelines](https://github.com/luarocks/lua-style-guide)

While it is mandatory to follow those guidelines for modules, they will not be enforced on community powered Stream Connectors scripts.
<<<<<<< HEAD
It is however recommened to follow them as much as possible.

## Documentations

When creating a module you must comment your methods as follow
=======
It is however recommened to follow them as much as possible. 

## Documentations

When creating a module you must comment your methods as follow 
>>>>>>> centreon-stream-connector-scripts/feat(streamconnector)-add-refacto-bsm-v2-lua-files

```lua
--- This is a local function that does things
-- @param first_name (string) the first name 
-- @param last_name (string) the last name
-- @return age (number) the age of the person
local function get_age(first_name, last_name)
  -- some code --
end
```

<<<<<<< HEAD
You should comment complicated or long code blocks to help people review your code.
=======
You should comment complicated or long code blocks to help people review your code. 
>>>>>>> centreon-stream-connector-scripts/feat(streamconnector)-add-refacto-bsm-v2-lua-files

It is also required to create or update the module documentation for a more casual reading to help people use your module in their Stream Connector
