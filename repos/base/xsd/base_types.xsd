<?xml version="1.0"?>
<xs:schema xmlns:xs="http://www.w3.org/2001/XMLSchema">

	<xs:simpleType name="Boolean">
		<xs:restriction base="xs:string">
			<xs:enumeration value="true" />
			<xs:enumeration value="yes" />
			<xs:enumeration value="on" />
			<xs:enumeration value="false" />
			<xs:enumeration value="no" />
			<xs:enumeration value="off" />
		</xs:restriction>
	</xs:simpleType><!-- Boolean -->

	<xs:simpleType name="Trace_policy_name">
		<xs:restriction base="xs:string">
			<xs:minLength value="1"/>
			<xs:maxLength value="40"/>
		</xs:restriction>
	</xs:simpleType><!-- Trace_policy_name -->

	<xs:simpleType name="Thread_name">
		<xs:restriction base="xs:string">
			<xs:minLength value="1"/>
			<xs:maxLength value="40"/>
		</xs:restriction>
	</xs:simpleType><!-- Thread_name -->

	<xs:simpleType name="Number_of_bytes">
		<xs:restriction base="xs:string">
			<xs:minLength value="1"/>
		</xs:restriction>
	</xs:simpleType><!-- Number_of_bytes -->

	<xs:simpleType name="Session_label">
		<xs:restriction base="xs:string">
			<xs:minLength value="1"/>
			<xs:maxLength value="160"/>
		</xs:restriction>
	</xs:simpleType><!-- Session_label -->

	<xs:simpleType name="Seconds">
		<xs:restriction base="xs:integer">
			<xs:minInclusive value="1"/>
			<xs:maxInclusive value="3600"/>
		</xs:restriction>
	</xs:simpleType><!-- Seconds -->

	<xs:simpleType name="Mac_address">
		<xs:restriction base="xs:string">
			<xs:pattern value="[0-9a-fA-F]{2}.[0-9a-fA-F]{2}.[0-9a-fA-F]{2}.[0-9a-fA-F]{2}.[0-9a-fA-F]{2}.[0-9a-fA-F]{2}"/>
		</xs:restriction>
	</xs:simpleType><!-- Mac_address -->

	<xs:simpleType name="Ipv4_address">
		<xs:restriction base="xs:string">
			<xs:pattern value="[0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}"/>
		</xs:restriction>
	</xs:simpleType><!-- Ipv4_address -->

	<xs:simpleType name="Ipv4_address_prefix">
		<xs:restriction base="xs:string">
			<xs:pattern value="[0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}.[0-9]{1,3}/[0-9]{1,2}"/>
		</xs:restriction>
	</xs:simpleType><!-- Ipv4_address_prefix -->

	<xs:simpleType name="Port">
		<xs:restriction base="xs:integer">
			<xs:minInclusive value="0"/>
			<xs:maxInclusive value="65535"/>
		</xs:restriction>
	</xs:simpleType><!-- Port -->

</xs:schema>
