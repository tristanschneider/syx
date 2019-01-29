-- script.lua
-- Receives a table, returns the sum of its components.
function func(input)
  print("Called lua function with input ", input);
  local result = testC(1);
  print("Result from C ", result);
end

local function doTest()
  print("The table the script received has:\n");
  local x = 0
  for i = 1, #foo do
    print(i, foo[i])
    x = x + foo[i]
  end
  print("Returning data back to C\n");
  return x
end
return doTest();