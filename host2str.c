/*
 * host2str.c
 *
 * conversion routines from the host format
 * to the presentation format (strings)
 *
 * a Net::DNS like library for C
 * 
 * (c) NLnet Labs, 2004
 *
 * See the file LICENSE for the license
 */
#include <config.h>

#include <limits.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ldns/host2str.h>
#include <ldns/wire2host.h>

#include <util.h>

/* lookup tables partly stolen from nsd, is there better way?
   are these used somewhere else? */

/* Taken from RFC 2538, section 2.1.  */
ldns_lookup_table ldns_certificate_types[] = {
        { 0, "PKIX" },  /* X.509 as per PKIX */
        { 1, "SPKI" },  /* SPKI cert */
        { 2, "PGP" },   /* PGP cert */
        { 253, "URI" }, /* URI private */
        { 254, "OID" }, /* OID private */
        { 0, NULL }
};

/* Taken from RFC 2535, section 7.  */
ldns_lookup_table ldns_algorithms[] = {
        { 1, "RSAMD5" },
        { 2, "DS" },
        { 3, "DSA" },
        { 4, "ECC" },
        { 5, "RSASHA1" },       
        { 252, "INDIRECT" },
        { 253, "PRIVATEDNS" },
        { 254, "PRIVATEOID" },
        { 0, NULL }
};

/* rr types (TODO: maybe these should be in rr.c? add enum? */
ldns_lookup_table ldns_rr_classes[] = {
	{ LDNS_RR_CLASS_IN, "IN" },
	{ LDNS_RR_CLASS_CHAOS, "CH" },
	{ LDNS_RR_CLASS_HS, "HS" },
	{ LDNS_RR_CLASS_ANY, "ANY" },
	{ 0, NULL }
};

/* if these are used elsewhere, move to packet.c? */
ldns_lookup_table ldns_rcodes[] = {
	{ 0, "NOERROR" },
	{ 1, "FORMERR" },
	{ 2, "SERVFAIL" },
	{ 3, "NAMEERR" },
	{ 4, "NOTIMPL" },
	{ 5, "REFUSED" },
	{ 0, NULL }
};

ldns_lookup_table ldns_opcodes[] = {
	{ 0, "QUERY" },
	{ 1, "IQUERY" },
	{ 2, "STATUS" },
	{ 0, NULL }
};

/* do NOT pass compressed data here :p */
ldns_status
ldns_rdf2buffer_str_dname(ldns_buffer *output, ldns_rdf *dname)
{
	/* can we do with 1 pos var? or without at all? */
	uint8_t src_pos = 0;
	uint8_t len;
	uint8_t *data;
	
	data = (uint8_t*)ldns_rdf_data(dname);
	len = data[src_pos];

	/* single root label */
	if (1 == ldns_rdf_size(dname)) {
		ldns_buffer_printf(output, ".", 2);
	} else {
		/* XXX repeated calls to ldns_rdf_size */
		while ((len > 0) && src_pos < ldns_rdf_size(dname)) {
			src_pos++;
			ldns_buffer_write(output, &data[src_pos], len);
			src_pos += len;
			len = data[src_pos];
			ldns_buffer_printf(output, ".");
		}
	}
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_int8(ldns_buffer *output, ldns_rdf *rdf)
{
	uint8_t data = ldns_rdf_data(rdf)[0];
	ldns_buffer_printf(output, "%lu", (unsigned long) data);
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_int16(ldns_buffer *output, ldns_rdf *rdf)
{
	uint16_t data = read_uint16(ldns_rdf_data(rdf));
	ldns_buffer_printf(output, "%lu", (unsigned long) data);
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_int32(ldns_buffer *output, ldns_rdf *rdf)
{
	uint32_t data = read_uint32(ldns_rdf_data(rdf));
	ldns_buffer_printf(output, "%lu", (unsigned long) data);
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_time(ldns_buffer *output, ldns_rdf *rdf)
{
	uint32_t data = read_uint32(ldns_rdf_data(rdf));
	ldns_buffer_printf(output, "%lu", (unsigned long) data);
	return ldns_buffer_status(output);
}

/** 
 * Converts A address 
 */
ldns_status
ldns_rdf2buffer_str_a(ldns_buffer *output, ldns_rdf *rdf)
{
	char str[INET_ADDRSTRLEN];
	
	if (inet_ntop(AF_INET, ldns_rdf_data(rdf), str, INET_ADDRSTRLEN)) {
		ldns_buffer_printf(output, "%s", str);
	}
	return ldns_buffer_status(output);
}

/** 
 * converts AAAA address 
 */
ldns_status
ldns_rdf2buffer_str_aaaa(ldns_buffer *output, ldns_rdf *rdf)
{
	char str[INET6_ADDRSTRLEN];

	if (inet_ntop(AF_INET6, ldns_rdf_data(rdf), str, (socklen_t) sizeof(str))) {
		ldns_buffer_printf(output, "%s", str);
	}

	return ldns_buffer_status(output);
}

/**
 * Converts TXT rdata
 */
ldns_status
ldns_rdf2buffer_str_str(ldns_buffer *output, ldns_rdf *rdf)
{
	const uint8_t *data = ldns_rdf_data(rdf);
	uint8_t length = data[0];
	size_t i;

	ldns_buffer_printf(output, "\"");
	for (i = 1; i <= length; ++i) {
		char ch = (char) data[i];
		if (isprint(ch)) {
			if (ch == '"' || ch == '\\') {
				ldns_buffer_printf(output, "\\");
			}
			ldns_buffer_printf(output, "%c", ch);
		} else {
			ldns_buffer_printf(output, "\\%03u", (unsigned) ch);
		}
	}
	ldns_buffer_printf(output, "\"");
	return ldns_buffer_status(output);
}

/**
 * Converts Base 64 encoded data
 */
ldns_status
ldns_rdf2buffer_str_b64(ldns_buffer *output, ldns_rdf *rdf)
{
	/*ldns_buffer_printf(output, "%s", ldns_rdf_data(rdf));*/
	size_t size = (((ldns_rdf_size(rdf) + 2) / 3) * 4) + 1;
	char *b64 = XMALLOC(char, size);
	if (b64_ntop(ldns_rdf_data(rdf), ldns_rdf_size(rdf), b64, size)) {
		ldns_buffer_printf(output, "%s", b64);
	}
	FREE(b64);
	return ldns_buffer_status(output);
}	

/**
 * Converts Hex encoded data
 * move this to general func?
 */
ldns_status
ldns_rdf2buffer_str_hex(ldns_buffer *output, ldns_rdf *rdf)
{
/*
	char hex_chars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8',
	                     '9', 'A', 'B', 'C', 'D', 'E', 'F' };

	size_t i;
ldns_buffer_printf(output, "HEX: ");
	for (i = 0; i < ldns_rdf_size(rdf); i++) {
		ldns_buffer_printf(output, "%c", hex_chars[ldns_rdf_data(rdf)[i] & 0xF0]);
		ldns_buffer_printf(output, "%c", hex_chars[ldns_rdf_data(rdf)[i] & 0x0F]);
	}
*/
	size_t i;
	for (i = 0; i < ldns_rdf_size(rdf); i++) {
		ldns_buffer_printf(output, "%02x", ldns_rdf_data(rdf)[i]);
	}

	return ldns_buffer_status(output);
}	

/**
 * Converts type encoded data
 */
ldns_status
ldns_rdf2buffer_str_type(ldns_buffer *output, ldns_rdf *rdf)
{
        uint16_t data = read_uint16(ldns_rdf_data(rdf));
	const ldns_rr_descriptor *descriptor;

	descriptor = ldns_rr_descript(data);
	if (descriptor->_name) {
		ldns_buffer_printf(output, "%s", descriptor->_name);
	} else {
		ldns_buffer_printf(output, "TYPE%u", data);
	}
	return ldns_buffer_status(output);
}	

/**
 * Converts class encoded data
 */
ldns_status
ldns_rdf2buffer_str_class(ldns_buffer *output, ldns_rdf *rdf)
{
        uint8_t data = ldns_rdf_data(rdf)[0];
	ldns_lookup_table *lt;

 	lt = ldns_lookup_by_id(ldns_rr_classes, (int) data);
	if (lt) {
		ldns_buffer_printf(output, "\t%s", lt->name);
	} else {
		ldns_buffer_printf(output, "\tCLASS%d", data);
	}
	return ldns_buffer_status(output);
}	

ldns_status
ldns_rdf2buffer_str_alg(ldns_buffer *output, ldns_rdf *rdf)
{
        uint8_t data = ldns_rdf_data(rdf)[0];
	ldns_lookup_table *lt;

 	lt = ldns_lookup_by_id(ldns_algorithms, (int) data);
	if (lt) {
		ldns_buffer_printf(output, "%s", lt->name);
	} else {
		ldns_buffer_printf(output, "ALG%d", data);
	}
	return ldns_buffer_status(output);
}	

ldns_status
ldns_rdf2buffer_str_cert(ldns_buffer *output, ldns_rdf *rdf)
{
        uint16_t data = read_uint16(ldns_rdf_data(rdf));
	ldns_lookup_table *lt;

 	lt = ldns_lookup_by_id(ldns_certificate_types, (int) data);
	if (lt) {
		ldns_buffer_printf(output, "%s", lt->name);
	} else {
		ldns_buffer_printf(output, "ALG%d", data);
	}
	return ldns_buffer_status(output);
}	

ldns_status
ldns_rdf2buffer_str_loc(ldns_buffer *output, ldns_rdf *rdf)
{
	/* we could do checking (ie degrees < 90 etc)? */
	uint8_t version = ldns_rdf_data(rdf)[0];
	uint8_t size;
	uint8_t horizontal_precision;
	uint8_t vertical_precision;
	uint32_t longitude;
	uint32_t latitude;
	uint32_t altitude;
	char northerness;
	char easterness;
	uint32_t h;
	uint32_t m;
	double s;
	long value, unit, meters;
	
	uint32_t equator = (uint32_t) power(2, 31);

	if (version == 0) {
		size = ldns_rdf_data(rdf)[1];
		horizontal_precision = ldns_rdf_data(rdf)[2];
		vertical_precision = ldns_rdf_data(rdf)[3];
		
		latitude = read_uint32(&ldns_rdf_data(rdf)[4]);
		longitude = read_uint32(&ldns_rdf_data(rdf)[8]);
		altitude = read_uint32(&ldns_rdf_data(rdf)[12]);
		
		if (latitude > equator) {
			northerness = 'N';
			latitude = latitude - equator;
		} else {
			northerness = 'S';
			latitude = equator - latitude;
		}
		h = latitude / (1000 * 60 * 60);
		latitude = latitude % (1000 * 60 * 60);
		m = latitude / (1000 * 60);
		latitude = latitude % (1000 * 60);
		s = (double) latitude / 1000.0;
		ldns_buffer_printf(output, "%02u %02u %0.3f %c ", h, m, s, northerness);

		if (longitude > equator) {
			easterness = 'E';
			longitude = longitude - equator;
		} else {
			easterness = 'W';
			longitude = equator - longitude;
		}
		h = longitude / (1000 * 60 * 60);
		longitude = longitude % (1000 * 60 * 60);
		m = longitude / (1000 * 60);
		longitude = longitude % (1000 * 60);
		s = (double) longitude / (1000.0);
		ldns_buffer_printf(output, "%02u %02u %0.3f %c ", h, m, s, easterness);

		meters = (long) altitude - 10000000;
		ldns_buffer_printf(output, "%u", meters / 100);
		if (meters % 100 != 0) {
			ldns_buffer_printf(output, ".%02u", meters % 100);
		}
		ldns_buffer_printf(output, "m ");
		
		value = (short) ((size & 0xf0) >> 4);
		unit = (short) (size & 0x0f);
		meters = value * power(10, unit);
		ldns_buffer_printf(output, "%u", meters / 100);
		if (meters % 100 != 0) {
			ldns_buffer_printf(output, ".%02u", meters % 100);
		}
		ldns_buffer_printf(output, "m ");

		value = (short) ((horizontal_precision & 0xf0) >> 4);
		unit = (short) (horizontal_precision & 0x0f);
		meters = value * power(10, unit);
		ldns_buffer_printf(output, "%u", meters / 100);
		if (meters % 100 != 0) {
			ldns_buffer_printf(output, ".%02u", meters % 100);
		}
		ldns_buffer_printf(output, "m ");

		value = (long) ((vertical_precision & 0xf0) >> 4);
		unit = (long) (vertical_precision & 0x0f);
		meters = value * power(10, unit);
		ldns_buffer_printf(output, "%u", meters / 100);
		if (meters % 100 != 0) {
			ldns_buffer_printf(output, ".%02u", meters % 100);
		}
		ldns_buffer_printf(output, "m ");

		return ldns_buffer_status(output);
	} else {
		return ldns_rdf2buffer_str_hex(output, rdf);
	}
}

ldns_status
ldns_rdf2buffer_str_unknown(ldns_buffer *output, ldns_rdf *rdf)
{
	ldns_buffer_printf(output, "\\# %u ", ldns_rdf_size(rdf));
	return ldns_rdf2buffer_str_hex(output, rdf);
}

ldns_status
ldns_rdf2buffer_str_nsap(ldns_buffer *output, ldns_rdf *rdf)
{
	ldns_buffer_printf(output, "0x");
	return ldns_rdf2buffer_str_hex(output, rdf);
}

ldns_status
ldns_rdf2buffer_str_wks(ldns_buffer *output, ldns_rdf *rdf)
{
	/* protocol, followed by bitmap of services */
	struct protoent *protocol;
	char *proto_name = NULL;
	uint8_t protocol_nr;
	struct servent *service;
	uint16_t current_service;

	protocol_nr = ldns_rdf_data(rdf)[0];
	protocol = getprotobynumber((int) protocol_nr);
	if (protocol && (protocol->p_name != NULL)) {
		proto_name = protocol->p_name;
		ldns_buffer_printf(output, "%s ", protocol->p_name);
	} else {
		ldns_buffer_printf(output, "%u ", protocol_nr);
	}

	for (current_service = 0; 
	     current_service < ldns_rdf_size(rdf) * 8;
	     current_service++) {
		if (get_bit(&(ldns_rdf_data(rdf)[1]), current_service)) {
			service = getservbyport((int) ntohs(current_service),
			                        proto_name);
			if (service && service->s_name) {
				ldns_buffer_printf(output, "%s ", 
				                   service->s_name);
			} else {
				ldns_buffer_printf(output, "%u ",
				                   current_service);
			}
		}
	}
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_nsec(ldns_buffer *output, ldns_rdf *rdf)
{
	uint8_t window_block_nr;
	uint8_t bitmap_length;
	uint16_t type;
	uint16_t pos = 0;
	uint16_t bit_pos;
	uint8_t *data = ldns_rdf_data(rdf);
	const ldns_rr_descriptor *descriptor;
	
	while(pos < ldns_rdf_size(rdf)) {
		window_block_nr = data[pos];
		bitmap_length = data[pos+1];
		pos += 2;
		

		for (bit_pos = 0; bit_pos < (bitmap_length) * 8; bit_pos++) {
			if (get_bit(&data[pos], bit_pos)) {
				type = 256 * (uint16_t) window_block_nr + bit_pos;
				descriptor = ldns_rr_descript(type);

				if (descriptor->_name) {
					ldns_buffer_printf(output, "%s ", descriptor->_name);
				} else {
					ldns_buffer_printf(output, "TYPE%d ", type);
				}
			}
		}
		
		pos += (uint16_t) bitmap_length;
	}
	
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_tsigtime(ldns_buffer *output, ldns_rdf *rdf)
{
	/* tsigtime is 48 bits network order unsigned integer */
	uint64_t result;
	uint8_t tsigtime[8];
	tsigtime[0] = 0;
	tsigtime[1] = 0;
	memcpy(&tsigtime[2], ldns_rdf_data(rdf), 6);
	result = (uint64_t) tsigtime;
	
	ldns_buffer_printf(output, "%u ", ntohl(result));
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_apl(ldns_buffer *output, ldns_rdf *rdf)
{
	uint8_t *data = ldns_rdf_data(rdf);
	uint16_t address_family = read_uint16(data);
	uint8_t prefix = data[2];
	bool negation;
	uint8_t adf_length;
	unsigned short i;
	unsigned int pos = 0;
	
/*
	printf("address family: %u\n", address_family);
	printf("prefix: %u\n", prefix);
	printf("negation: %u\n", negation);
	printf("adf length: %u\n", adf_length);
*/	
	/* todo: use #defines for address families? */
	
	/* ipv4 */
	while (pos < (unsigned int) ldns_rdf_size(rdf)) {
		address_family = read_uint16(&data[pos]);
		prefix = data[pos + 2];
		negation = data[pos + 3] & 0x80;
		adf_length = data[pos + 3] & 0x7f;
		if (address_family == 1) {
			/* check if prefix < 32? */
			if (negation) {
				ldns_buffer_printf(output, "!");
			}
			ldns_buffer_printf(output, "%u:", address_family);
			/* address is variable length 0 - 4 */
			for (i = 0; i < 4; i++) {
				if (i > 0) {
					ldns_buffer_printf(output, ".");
				}
				if (i < (unsigned short) adf_length) {
					ldns_buffer_printf(output, "%d", 
					                   data[pos + i + 4]);
				} else {
					ldns_buffer_printf(output, "0");
				}
			}
			ldns_buffer_printf(output, "/%u ", prefix);
		} else if (address_family == 2) {
			/* check if prefix < 128? */
			if (negation) {
				ldns_buffer_printf(output, "!");
			}
			ldns_buffer_printf(output, "%u:", address_family);
			/* address is variable length 0 - 16 */
			for (i = 0; i < 16; i++) {
				if (i % 2 == 0 && i > 0) {
					ldns_buffer_printf(output, ":");
				}
				if (i < (unsigned short) adf_length) {
					ldns_buffer_printf(output, "%02x", 
					                   data[pos + i + 4]);
				} else {
					ldns_buffer_printf(output, "00");
				}
			}
			ldns_buffer_printf(output, "/%u ", prefix);
		
		} else {
			/* unknown address family */
			ldns_buffer_printf(output, "Unknown address family: %u data: ", address_family);
			for (i = 1; i < (unsigned short) (4 + adf_length); i++) {
				ldns_buffer_printf(output, "%02x", data[i]);
			}
		}
		pos += 4 + adf_length;
	}
	return ldns_buffer_status(output);
}

ldns_status
ldns_rdf2buffer_str_todo(ldns_buffer *output, ldns_rdf *rdf)
{
	(void) ldns_rdf_data(rdf);
	ldns_buffer_printf(output, "todo: ");
	return ldns_rdf2buffer_str_hex(output, rdf);
}

ldns_status
ldns_rdf2buffer_str_ipseckey(ldns_buffer *output, ldns_rdf *rdf)
{
	/* wire format from 
	   http://www.ietf.org/internet-drafts/draft-ietf-ipseckey-rr-12.txt
	*/
	uint8_t *data = ldns_rdf_data(rdf);
	uint8_t precedence;
	uint8_t gateway_type;
	uint8_t algorithm;
	
	ldns_rdf *gateway;
	uint8_t *gateway_data;
	
	size_t public_key_size;
	uint8_t *public_key_data;
	ldns_rdf *public_key;
	
	size_t offset = 0;
	ldns_status status;
	
	
	precedence = data[0];
	gateway_type = data[1];
	algorithm = data[2];
	offset = 3;
	
	switch (gateway_type) {
		case 0:
			/* no gateway */
			break;
		case 1:
			gateway_data = XMALLOC(uint8_t, 4);
			memcpy(gateway_data, &data[offset], 4);
			gateway = ldns_rdf_new(4, LDNS_RDF_TYPE_A, gateway_data);
			break;
		case 2:
			gateway_data = XMALLOC(uint8_t, 16);
			memcpy(gateway_data, &data[offset], 16);
			gateway = ldns_rdf_new(16, LDNS_RDF_TYPE_AAAA, gateway_data);
			break;
		case 3:
			status = ldns_wire2dname(&gateway, data, ldns_rdf_size(rdf), &offset);
			break;
		default:
			/* error? */
			break;
	}

	public_key_size = ldns_rdf_size(rdf) - offset;
	public_key_data = XMALLOC(uint8_t, public_key_size);
	memcpy(public_key_data, &data[offset], public_key_size);
	public_key = ldns_rdf_new(public_key_size, LDNS_RDF_TYPE_B64, public_key_data);
	
	ldns_buffer_printf(output, "%u %u %u ", precedence, gateway_type,
	                   algorithm);
	(void) ldns_rdf2buffer_str(output, gateway);
	ldns_buffer_printf(output, " ");
	(void) ldns_rdf2buffer_str(output, public_key);	

	ldns_rdf_free(gateway);
	ldns_rdf_free(public_key);
	
	return ldns_buffer_status(output);
}

/**
 * Returns string representation of the specified rdf
 * Data is not static
 */
ldns_status
ldns_rdf2buffer_str(ldns_buffer *buffer, ldns_rdf *rdf)
{
	ldns_status res;

	if (rdf) {
		switch(ldns_rdf_get_type(rdf)) {
		case LDNS_RDF_TYPE_NONE:
			break;
		case LDNS_RDF_TYPE_DNAME:
			res = ldns_rdf2buffer_str_dname(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_INT8:
			res = ldns_rdf2buffer_str_int8(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_INT16:
			res = ldns_rdf2buffer_str_int16(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_INT32:
			res = ldns_rdf2buffer_str_int32(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_TSIGTIME:
			res = ldns_rdf2buffer_str_tsigtime(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_A:
			res = ldns_rdf2buffer_str_a(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_AAAA:
			res = ldns_rdf2buffer_str_aaaa(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_STR:
			res = ldns_rdf2buffer_str_str(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_APL:
			res = ldns_rdf2buffer_str_apl(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_B64:
			res = ldns_rdf2buffer_str_b64(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_HEX:
			res = ldns_rdf2buffer_str_hex(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_NSEC: 
			res = ldns_rdf2buffer_str_nsec(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_TYPE: 
			res = ldns_rdf2buffer_str_type(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_CLASS:
			res = ldns_rdf2buffer_str_class(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_CERT:
			res = ldns_rdf2buffer_str_cert(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_ALG:
			res = ldns_rdf2buffer_str_alg(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_UNKNOWN:
			res = ldns_rdf2buffer_str_unknown(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_TIME:
			res = ldns_rdf2buffer_str_time(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_LOC:
			res = ldns_rdf2buffer_str_loc(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_WKS:
			res = ldns_rdf2buffer_str_wks(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_NSAP:
			res = ldns_rdf2buffer_str_nsap(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_IPSECKEY:
			res = ldns_rdf2buffer_str_ipseckey(buffer, rdf);
			break;
		case LDNS_RDF_TYPE_SERVICE:
			/* XXX todo */
			break;
		}
	}

	return LDNS_STATUS_OK;
}

/* TODO status */
ldns_status
ldns_rr2buffer_str(ldns_buffer *output, ldns_rr *rr)
{
	uint16_t i;
	ldns_status status = LDNS_STATUS_OK;
	ldns_lookup_table *lt;
	const ldns_rr_descriptor *descriptor;
	
	if (ldns_rr_owner(rr)) {
		status = ldns_rdf2buffer_str_dname(output, ldns_rr_owner(rr));  
	}
	if (status != LDNS_STATUS_OK) {
		return status;
	}
	
 	lt = ldns_lookup_by_id(ldns_rr_classes, ldns_rr_get_class(rr));
	if (lt) {
		ldns_buffer_printf(output, "\t%s\t", lt->name);
	} else {
		ldns_buffer_printf(output, "\tCLASS%d\t", ldns_rr_get_class(rr));
	}

	descriptor = ldns_rr_descript(ldns_rr_get_type(rr));

	if (descriptor->_name) {
		ldns_buffer_printf(output, "%s\t", descriptor->_name);
	} else {
		/* exceptions for qtype */
		if (ldns_rr_get_type(rr) == 251) {
			ldns_buffer_printf(output, "IXFR ");
		} else if (ldns_rr_get_type(rr) == 252) {
			ldns_buffer_printf(output, "AXFR ");
		} else if (ldns_rr_get_type(rr) == 253) {
			ldns_buffer_printf(output, "MAILB ");
		} else if (ldns_rr_get_type(rr) == 254) {
			ldns_buffer_printf(output, "MAILA ");
		} else if (ldns_rr_get_type(rr) == 255) {
			ldns_buffer_printf(output, "ANY ");
		} else {
			ldns_buffer_printf(output, "TYPE%d\t", ldns_rr_get_type(rr));
		}
	}
	
	for (i = 0; i < ldns_rr_rd_count(rr); i++) {
		status = ldns_rdf2buffer_str(output, ldns_rr_rdf(rr, i));
		ldns_buffer_printf(output, " ");
	}
	
	return ldns_buffer_status(output);
}

/**
 * Prints the header in default format in the given buffer
 */
ldns_status
ldns_pktheader2buffer_str(ldns_buffer *output, ldns_pkt *pkt)
{
	/* TODO: strings for known names instead of numbers, flags etc */
	const char *opcode_str, *rcode_str;
	ldns_lookup_table *opcode = ldns_lookup_by_id(ldns_opcodes,
			                    (int) ldns_pkt_opcode(pkt));
	ldns_lookup_table *rcode = ldns_lookup_by_id(ldns_rcodes,
			                    (int) ldns_pkt_rcode(pkt));

	if (opcode) {
		opcode_str = opcode->name;
	} else {
		opcode_str = "??";
	}
	if (rcode) {
		rcode_str = rcode->name;
	} else {
		rcode_str = "??";
	}
	
	ldns_buffer_printf(output, ";; ->>HEADER<<- ");
	ldns_buffer_printf(output, "opcode: %s, ", opcode_str);
	ldns_buffer_printf(output, "rcode: %s, ", rcode_str);
	ldns_buffer_printf(output, "id %lu\n", ldns_pkt_id(pkt));
	ldns_buffer_printf(output, ";; flags: ");

	if (ldns_pkt_qr(pkt)) {
		ldns_buffer_printf(output, "qr ");
	}
	if (ldns_pkt_aa(pkt)) {
		ldns_buffer_printf(output, "aa ");
	}
	if (ldns_pkt_tc(pkt)) {
		ldns_buffer_printf(output, "tc ");
	}
	if (ldns_pkt_rd(pkt)) {
		ldns_buffer_printf(output, "rd ");
	}
	if (ldns_pkt_cd(pkt)) {
		ldns_buffer_printf(output, "cd ");
	}
	if (ldns_pkt_ra(pkt)) {
		ldns_buffer_printf(output, "ra ");
	}
	if (ldns_pkt_ad(pkt)) {
		ldns_buffer_printf(output, "ad ");
	}
	ldns_buffer_printf(output, "; ");

	ldns_buffer_printf(output, "QUERY: %u, ", ldns_pkt_qdcount(pkt));
	ldns_buffer_printf(output, "ANSWER: %u, ", ldns_pkt_ancount(pkt));
	ldns_buffer_printf(output, "AUTHORITY: %u, ", ldns_pkt_nscount(pkt));
	ldns_buffer_printf(output, "ADDITIONAL: %u, ", ldns_pkt_arcount(pkt));

	return ldns_buffer_status(output);
}
/* TODO check status returns */

ldns_status
ldns_pkt2buffer_str(ldns_buffer *output, ldns_pkt *pkt)
{
	uint16_t i;
	ldns_status status = LDNS_STATUS_OK;
	
	if (ldns_buffer_status_ok(output)) {
		status = ldns_pktheader2buffer_str(output, pkt);
		if (status != LDNS_STATUS_OK) {
			/*printf("error in pkt2buf %d\n", status);*/
			return status;
		}
		
		ldns_buffer_printf(output, "\n");
		
		ldns_buffer_printf(output, ";; QUESTION SECTION:\n;; ");

		for (i = 0; i < ldns_pkt_qdcount(pkt); i++) {
			status = ldns_rr2buffer_str(output, 
				       ldns_rr_list_rr(ldns_pkt_question(pkt), i));
			if (status != LDNS_STATUS_OK) {
				return status;
			}

			ldns_buffer_printf(output, "\n");
		}
		ldns_buffer_printf(output, "\n");
		
		ldns_buffer_printf(output, ";; ANSWER SECTION:\n");
		for (i = 0; i < ldns_pkt_ancount(pkt); i++) {
			status = ldns_rr2buffer_str(output, 
				       ldns_rr_list_rr(ldns_pkt_answer(pkt), i));
			if (status != LDNS_STATUS_OK) {
				return status;
			}

			ldns_buffer_printf(output, "\n");
		}
		ldns_buffer_printf(output, "\n");
		
		ldns_buffer_printf(output, ";; AUTHORITY SECTION:\n");

		for (i = 0; i < ldns_pkt_nscount(pkt); i++) {
			status = ldns_rr2buffer_str(output, 
				       ldns_rr_list_rr(ldns_pkt_authority(pkt), i));
			if (status != LDNS_STATUS_OK) {
				return status;
			}
			ldns_buffer_printf(output, "\n");
		}
		ldns_buffer_printf(output, "\n");
		
		ldns_buffer_printf(output, ";; ADDITIONAL SECTION:\n");
		for (i = 0; i < ldns_pkt_arcount(pkt); i++) {
			status = ldns_rr2buffer_str(output, 
				       ldns_rr_list_rr(ldns_pkt_additional(pkt), i));
			if (status != LDNS_STATUS_OK) {
				return status;
			}

			ldns_buffer_printf(output, "\n");
		}
		ldns_buffer_printf(output, "\n");
		/* add some futher fields */
		ldns_buffer_printf(output, ";; Query time: %d msec\n", ldns_pkt_querytime(pkt));
		ldns_buffer_printf(output, ";; SERVER: %s\n", ldns_rdf2str(ldns_pkt_answerfrom(pkt))); 
	} else {
		return ldns_buffer_status(output);
	}
	return status;
}

/*
 * Zero terminate the buffer and fix it to the size of the string.
 */
char *
buffer2str(ldns_buffer *buffer)
{
	if (!ldns_buffer_reserve(buffer, 1)) {
		return NULL;
	}
	ldns_buffer_write_u8(buffer, (uint8_t) '\0');
	if (!ldns_buffer_set_capacity(buffer, ldns_buffer_position(buffer))) {
		return NULL;
	}
	return ldns_buffer_export(buffer);
}

char *
ldns_rdf2str(ldns_rdf *rdf)
{
	char *result = NULL;
	ldns_buffer *tmp_buffer = ldns_buffer_new(1000);

	if (ldns_rdf2buffer_str(tmp_buffer, rdf) == LDNS_STATUS_OK) {
		/* export and return string, destroy rest */
		result = buffer2str(tmp_buffer);
	}
	
	ldns_buffer_free(tmp_buffer);
	return result;
}

char *
ldns_rr2str(ldns_rr *rr)
{
	char *result = NULL;
	ldns_buffer *tmp_buffer = ldns_buffer_new(1000);

	if (ldns_rr2buffer_str(tmp_buffer, rr) == LDNS_STATUS_OK) {
		/* export and return string, destroy rest */
		result = buffer2str(tmp_buffer);
	}
	
	ldns_buffer_free(tmp_buffer);
	return result;
}

char *
ldns_pkt2str(ldns_pkt *pkt)
{
	char *result = NULL;
	ldns_buffer *tmp_buffer = ldns_buffer_new(65535);

	if (ldns_pkt2buffer_str(tmp_buffer, pkt) == LDNS_STATUS_OK) {
		/* export and return string, destroy rest */
		result = buffer2str(tmp_buffer);
	}

	ldns_buffer_free(tmp_buffer);
	return result;
}

void
ldns_rdf_print(FILE *output, ldns_rdf *rdf)
{
	char *str = ldns_rdf2str(rdf);
	if (str) {
		fprintf(output, str);
	} else {
		fprintf(output, "Unable to convert rdf to string\n");
	}
}

void
ldns_rr_print(FILE *output, ldns_rr *rr)
{
	char *str = ldns_rr2str(rr);
	if (str) {
		fprintf(output, str);
	} else {
		fprintf(output, "Unable to convert rr to string\n");
	}
}

void
ldns_pkt_print(FILE *output, ldns_pkt *pkt)
{
	char *str = ldns_pkt2str(pkt);
	if (str) {
		fprintf(output, str);
	} else {
		fprintf(output, "Unable to convert packet to string\n");
	}
}

