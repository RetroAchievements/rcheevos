#ifndef TEST_CLIENT_H
#define TEST_CLIENT_H

void test_client();
void test_client_do_frame();
void test_client_load();
void test_client_user();

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
void test_client_external();
#endif

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
void test_client_raintegration();
#endif

#endif
