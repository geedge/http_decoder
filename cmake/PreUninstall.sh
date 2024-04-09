if [ $1 == 0 ]; then
	DST=${RPM_INSTALL_PREFIX}/sapp/stellar_plugin
	sed -i -n '$!N;/http_decoder.so/!P;D' ${DST}/spec.toml
	sed -i '/http_decoder_init/d' ${DST}/spec.toml
	sed -i '/http_decoder_exit/d' ${DST}/spec.toml
fi
