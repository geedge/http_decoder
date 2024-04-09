DST=${RPM_INSTALL_PREFIX}/sapp/

mkdir -p ${DST}/stellar_plugin
touch ${DST}/stellar_plugin/spec.toml

if ! grep -q '^\./plug/stellar_plugin/http_decoder.so' "${DST}/stellar_plugin/spec.toml"; then
    echo -e "\n" >> "${DST}/stellar_plugin/spec.toml"
    echo -e "[[plugin]]" >> "${DST}/stellar_plugin/spec.toml"
    echo -e "path = \"./stellar_plugin/http_decoder/http_decoder.so\"" >> "${DST}/stellar_plugin/spec.toml"
    echo -e "init = \"http_decoder_init\"" >> "${DST}/stellar_plugin/spec.toml"
    echo -e "exit = \"http_decoder_exit\"" >> "${DST}/stellar_plugin/spec.toml"
    echo -e "\n" >> "${DST}/stellar_plugin/spec.toml"
fi
