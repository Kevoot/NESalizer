
uint8_t read_button_states(unsigned n);
uint8_t set_button_state(unsigned n, unsigned i);
uint8_t clear_button_state(unsigned n, unsigned i);

template<bool calculating_size, bool is_save>
void transfer_input_state(uint8_t *&buf);
