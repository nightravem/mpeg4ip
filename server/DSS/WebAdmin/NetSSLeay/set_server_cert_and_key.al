# NOTE: Derived from blib/lib/Net/SSLeay.pm.
# Changes made here will be lost when autosplit again.
# See AutoSplit.pm.
package Net::SSLeay;

#line 1317 "blib/lib/Net/SSLeay.pm (autosplit into blib/lib/auto/Net/SSLeay/set_server_cert_and_key.al)"
###
### Easy set up of private key and certificate
###

sub set_server_cert_and_key {
    my ($ctx, $cert_path, $key_path) = @_;    
    my $errs = '';
    # Following will ask password unless private key is not encrypted
    CTX_use_RSAPrivateKey_file ($ctx, $key_path, &FILETYPE_PEM);
    $errs .= print_errs("private key `$key_path' ($!)");
    CTX_use_certificate_file ($ctx, $cert_path, &FILETYPE_PEM);
    $errs .= print_errs("certificate `$cert_path' ($!)");
    return wantarray ? (undef, $errs) : ($errs eq '');
}

# end of Net::SSLeay::set_server_cert_and_key
1;
