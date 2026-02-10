static int myphy_config_init(struct phy_device *phydev) {
    phy_write(phydev, 0x1f, 0x0007);
    phy_write(phydev, 0x10, 0x1234);
    return 0;
}

static struct phy_driver myphy_driver = {
    .phy_id = 0x001cc915,
    .phy_id_mask = 0xfffffff0,
    .name = "My PHY",
    .config_init = myphy_config_init,
};
