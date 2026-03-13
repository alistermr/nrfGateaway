#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>

/*ADDED INCLUDES*/
#include <zephyr/shell/shell.h>
#include <zephyr/bluetooth/mesh/shell.h>

static struct bt_mesh_cfg_cli cfg_cli;
static struct bt_mesh_health_srv_cb health_srv_cb;

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_SHELL_HEALTH_PUB_DEFINE(health_pub);

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0,
		     BT_MESH_MODEL_LIST(
				 BT_MESH_MODEL_CFG_SRV,
				 BT_MESH_MODEL_CFG_CLI(&cfg_cli),
			     BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	return &comp;
}