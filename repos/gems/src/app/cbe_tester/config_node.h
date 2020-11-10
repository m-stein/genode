


class Config_node
{
	private:

		bool const _verbose_cmd_pool_cmd_pending    ;
		bool const _verbose_cmd_pool_cmd_in_progress;
		bool const _verbose_cmd_pool_cmd_completed  ;
		bool const _verbose_blk_pkt_in_progress     ;
		bool const _verbose_blk_pkt_completed       ;
		bool const _verbose_ta_req_in_progress      ;
		bool const _verbose_ta_req_completed        ;
		bool const _verbose_crypto_req_completed    ;
		bool const _verbose_crypto_req_in_progress  ;
		bool const _verbose_client_data_mismatch    ;
		bool const _verbose_client_data_transferred ;

	public:

		Config_node(Xml_node const &node)
		:
			_verbose_cmd_pool_cmd_pending     { node.attribute_value("verbose_cmd_pool_cmd_pending"    , false) },
			_verbose_cmd_pool_cmd_in_progress { node.attribute_value("verbose_cmd_pool_cmd_in_progress", false) },
			_verbose_cmd_pool_cmd_completed   { node.attribute_value("verbose_cmd_pool_cmd_completed"  , false) },
			_verbose_blk_pkt_in_progress      { node.attribute_value("verbose_blk_pkt_in_progress"     , false) },
			_verbose_blk_pkt_completed        { node.attribute_value("verbose_blk_pkt_completed"       , false) },
			_verbose_ta_req_in_progress       { node.attribute_value("verbose_ta_req_in_progress"      , false) },
			_verbose_ta_req_completed         { node.attribute_value("verbose_ta_req_completed"        , false) },
			_verbose_crypto_req_completed     { node.attribute_value("verbose_crypto_req_completed"    , false) },
			_verbose_crypto_req_in_progress   { node.attribute_value("verbose_crypto_req_in_progress"  , false) },
			_verbose_client_data_mismatch     { node.attribute_value("verbose_client_data_mismatch"    , false) },
			_verbose_client_data_transferred  { node.attribute_value("verbose_client_data_transferred" , false) }
		{ }

		bool verbose_cmd_pool_cmd_pending    () const { return _verbose_cmd_pool_cmd_pending    ; }
		bool verbose_cmd_pool_cmd_in_progress() const { return _verbose_cmd_pool_cmd_in_progress; }
		bool verbose_cmd_pool_cmd_completed  () const { return _verbose_cmd_pool_cmd_completed  ; }
		bool verbose_blk_pkt_in_progress     () const { return _verbose_blk_pkt_in_progress     ; }
		bool verbose_blk_pkt_completed       () const { return _verbose_blk_pkt_completed       ; }
		bool verbose_ta_req_in_progress      () const { return _verbose_ta_req_in_progress      ; }
		bool verbose_ta_req_completed        () const { return _verbose_ta_req_completed        ; }
		bool verbose_crypto_req_completed    () const { return _verbose_crypto_req_completed    ; }
		bool verbose_crypto_req_in_progress  () const { return _verbose_crypto_req_in_progress  ; }
		bool verbose_client_data_mismatch    () const { return _verbose_client_data_mismatch    ; }
		bool verbose_client_data_transferred () const { return _verbose_client_data_transferred ; }
};


template <typename T>
T read_attribute(Xml_node const &node,
                 char     const *attr)
{
	T value { };

	if (!node.has_attribute(attr)) {

		error("<", node.type(), "> node misses attribute '", attr, "'");
		class Attribute_missing { };
		throw Attribute_missing();
	}
	if (!node.attribute(attr).value(value)) {

		error("<", node.type(), "> node has malformed '", attr, "' attribute");
		class Malformed_attribute { };
		throw Malformed_attribute();
	}
	return value;
}
