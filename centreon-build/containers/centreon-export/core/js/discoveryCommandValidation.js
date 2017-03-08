function discoveryCommandValidation(value) {
    var discoveryCommandRegexp = /^(((OID:(\.\d+)+=((?!;;).)+)|((UDP|TCP)=\d+(,\d+)*))((;;)|$))*$/;
    if (!discoveryCommandRegexp.test(value)) {
        return false;
    }

    return true;
}

/* export method for js unit test */
if (typeof module !== 'undefined' && module.exports != null) {
  exports.discoveryCommandValidation = discoveryCommandValidation;
}
