{pkgs ? import <nixpkgs> {}}: let
  lua-resty-websocket = pkgs.luajitPackages.callPackage (
    {
      buildLuarocksPackage,
      fetchFromGitHub,
      fetchurl,
      luaOlder,
    }:
      buildLuarocksPackage {
        pname = "lua-resty-websocket";
        version = "0.07-0";

        knownRockspec =
          (fetchurl {
            url = "https://luarocks.org/manifests/invizory/lua-resty-websocket-0.07-0.rockspec";
            hash = "sha256-VHwMvzh+JKbPvfcS1t8RB3PSQDDHO8ewpse7Q1qzvig=";
          }).outPath;

        src = fetchFromGitHub {
          owner = "openresty";
          repo = "lua-resty-websocket";
          tag = "v0.14";
          hash = "sha256-kzzoQ+wbPpOMFc57K5bRwGFZsWXVeK7DrNWfG0smAUM=";
        };

        disabled = luaOlder "5.1";

        meta = {
          homepage = "https://github.com/invizory/lua-resty-websocket";
          license.fullName = "MIT";
          description = "Lua WebSocket library for OpenResty.";
        };
      }
  ) {};
in
  pkgs.mkShell {
    buildInputs = [
      pkgs.openresty
      pkgs.luaPackages.lua-resty-http
      pkgs.luaPackages.lua-cjson
      pkgs.openssl
      lua-resty-websocket
    ];

    shellHook = ''
      LUA_PACKAGES_PATH="${pkgs.luaPackages.lua-resty-jwt}/share/lua/5.1/?.lua;${pkgs.luaPackages.lua-resty-jwt}/share/lua/5.1/?/init.lua"
    '';
  }
