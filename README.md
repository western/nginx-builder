# nginx-builder
Last Nginx with steroids

based on https://gist.github.com/western/c04efe49745f24874c43

## fixed versions
* nginx 1.16.1

* (openresty, memc-nginx-module.git) => github.com/openresty/memc-nginx-module.git
v0.19
* (chaoslawful, lua-nginx-module.git) => github.com/chaoslawful/lua-nginx-module.git
v0.10.15
* (simplresty, ngx_devel_kit.git) => github.com/simplresty/ngx_devel_kit.git
v0.3.1
* (openresty, redis2-nginx-module.git) => github.com/openresty/redis2-nginx-module.git
v0.15
* (openresty, echo-nginx-module.git) => github.com/openresty/echo-nginx-module.git
v0.61
* (calio, form-input-nginx-module.git) => github.com/calio/form-input-nginx-module.git
v0.12
* (openresty, set-misc-nginx-module.git) => github.com/openresty/set-misc-nginx-module.git
v0.32
* (Austinb, nginx-upload-module.git) => github.com/Austinb/nginx-upload-module.git
2.2.0
* (FRiCKLE, ngx_cache_purge.git) => github.com/FRiCKLE/ngx_cache_purge.git
2.3
* (openresty, headers-more-nginx-module.git) => github.com/openresty/headers-more-nginx-module.git
v0.33
* (nbs-system, naxsi.git) => github.com/nbs-system/naxsi.git
0.56
* (openresty, luajit2.git) => github.com/openresty/luajit2.git
v2.1-20190912


* (openresty, lua-resty-core.git) => github.com/openresty/lua-resty-core.git
v0.1.17
* (openresty, lua-resty-lrucache.git) => github.com/openresty/lua-resty-lrucache.git
v0.09
* (openresty, lua-cjson.git) => github.com/openresty/lua-cjson.git
2.1.0.7
* (openresty, lua-resty-redis.git) => github.com/openresty/lua-resty-redis.git
v0.27
* (cloudflare, lua-resty-cookie.git) => github.com/cloudflare/lua-resty-cookie.git
v0.1.0

## synopsis
* download release of package
* unpack
* edit ngx_install script for IS_LOCAL variable
* run ./ngx_install
