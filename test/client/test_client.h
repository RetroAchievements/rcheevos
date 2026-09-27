#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

void test_client();

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
void test_client_external();
#endif

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
void test_client_raintegration();
#endif

#endif
